#!/usr/bin/env python3
"""
Integration tests for the json-gen-c LSP server.

Launches the server via `json-gen-c --lsp` and verifies
initialize, diagnostics, completion, and shutdown responses.
"""

import json
import os
import subprocess
import sys

PASS = 0
FAIL = 0
SERVER_PATH = None


def find_server():
    """Locate the json-gen-c binary."""
    candidates = [
        os.path.join(os.path.dirname(__file__), '..', 'build', 'bin', 'json-gen-c'),
        os.path.join(os.path.dirname(__file__), '..', 'build', 'json-gen-c'),
    ]
    for c in candidates:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return os.path.abspath(c)
    return 'json-gen-c'  # hope it's in PATH


class LSPClient:
    def __init__(self, server_path):
        self.proc = subprocess.Popen(
            [server_path, '--lsp'],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    def send(self, msg):
        body = json.dumps(msg).encode()
        header = 'Content-Length: {}\r\n\r\n'.format(len(body)).encode()
        self.proc.stdin.write(header + body)
        self.proc.stdin.flush()

    def recv(self):
        line = b''
        while True:
            ch = self.proc.stdout.read(1)
            if not ch:
                return None
            line += ch
            if line.endswith(b'\r\n\r\n'):
                break
        for part in line.decode().split('\r\n'):
            if part.startswith('Content-Length:'):
                length = int(part.split(':')[1].strip())
                body = self.proc.stdout.read(length)
                return json.loads(body)
        return None

    def close(self):
        try:
            self.send({'jsonrpc': '2.0', 'id': 9999, 'method': 'shutdown', 'params': {}})
            self.recv()
            self.send({'jsonrpc': '2.0', 'method': 'exit'})
        except (BrokenPipeError, OSError):
            pass
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
        return self.proc.returncode


def check(label, condition):
    global PASS, FAIL
    if condition:
        PASS += 1
        print(f'  PASS: {label}')
    else:
        FAIL += 1
        print(f'  FAIL: {label}')


def test_initialize():
    print('Test: initialize handshake')
    c = LSPClient(SERVER_PATH)
    c.send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}})
    resp = c.recv()
    check('response has result', resp and 'result' in resp)
    check('has capabilities', 'capabilities' in resp.get('result', {}))
    caps = resp['result']['capabilities']
    check('textDocumentSync', 'textDocumentSync' in caps)
    check('completionProvider', 'completionProvider' in caps)
    check('hoverProvider', caps.get('hoverProvider') is True)
    check('serverInfo name', resp['result'].get('serverInfo', {}).get('name') == 'json-gen-c-lsp')
    rc = c.close()
    check('clean shutdown', rc == 0)


def test_diagnostics_error():
    print('Test: diagnostics for invalid schema')
    c = LSPClient(SERVER_PATH)
    c.send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}})
    c.recv()
    c.send({'jsonrpc': '2.0', 'method': 'initialized', 'params': {}})

    c.send({'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
        'textDocument': {
            'uri': 'file:///test.json-gen-c',
            'languageId': 'json-gen-c',
            'version': 1,
            'text': 'struct Foo { invalid_syntax }'
        }
    }})
    diag = c.recv()
    check('is publishDiagnostics', diag and diag.get('method') == 'textDocument/publishDiagnostics')
    diags = diag.get('params', {}).get('diagnostics', [])
    check('has at least one diagnostic', len(diags) > 0)
    if diags:
        check('severity is error (1)', diags[0].get('severity') == 1)
        check('source is json-gen-c', diags[0].get('source') == 'json-gen-c')
        check('has message', len(diags[0].get('message', '')) > 0)
    rc = c.close()
    check('clean shutdown', rc == 0)


def test_diagnostics_valid():
    print('Test: diagnostics for valid schema')
    c = LSPClient(SERVER_PATH)
    c.send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}})
    c.recv()

    c.send({'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
        'textDocument': {
            'uri': 'file:///valid.json-gen-c',
            'languageId': 'json-gen-c',
            'version': 1,
            'text': 'struct Foo {\n    int x;\n    sstr_t name;\n};'
        }
    }})
    diag = c.recv()
    diags = diag.get('params', {}).get('diagnostics', [])
    check('no diagnostics for valid schema', len(diags) == 0)
    rc = c.close()
    check('clean shutdown', rc == 0)


def test_diagnostics_update():
    print('Test: diagnostics update on didChange')
    c = LSPClient(SERVER_PATH)
    c.send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}})
    c.recv()

    # Open with error
    c.send({'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
        'textDocument': {
            'uri': 'file:///update.json-gen-c',
            'languageId': 'json-gen-c',
            'version': 1,
            'text': 'struct Bad {'
        }
    }})
    diag1 = c.recv()
    d1 = diag1.get('params', {}).get('diagnostics', [])
    check('error on initial open', len(d1) > 0)

    # Fix the error
    c.send({'jsonrpc': '2.0', 'method': 'textDocument/didChange', 'params': {
        'textDocument': {'uri': 'file:///update.json-gen-c', 'version': 2},
        'contentChanges': [{'text': 'struct Fixed {\n    int x;\n};'}]
    }})
    diag2 = c.recv()
    d2 = diag2.get('params', {}).get('diagnostics', [])
    check('no errors after fix', len(d2) == 0)

    rc = c.close()
    check('clean shutdown', rc == 0)


def test_completion():
    print('Test: textDocument/completion')
    c = LSPClient(SERVER_PATH)
    c.send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}})
    c.recv()

    c.send({'jsonrpc': '2.0', 'id': 2, 'method': 'textDocument/completion', 'params': {
        'textDocument': {'uri': 'file:///test.json-gen-c'},
        'position': {'line': 0, 'character': 0}
    }})
    resp = c.recv()
    items = resp.get('result', [])
    labels = [it['label'] for it in items]
    check('has struct completion', 'struct' in labels)
    check('has enum completion', 'enum' in labels)
    check('has oneof completion', 'oneof' in labels)
    check('has int type', 'int' in labels)
    check('has sstr_t type', 'sstr_t' in labels)
    check('has @json annotation', '@json' in labels)
    check('has @deprecated annotation', '@deprecated' in labels)
    check('has optional modifier', 'optional' in labels)
    check('has map keyword', 'map' in labels)
    check('has #include directive', '#include' in labels)
    rc = c.close()
    check('clean shutdown', rc == 0)


def test_hover():
    print('Test: textDocument/hover')
    c = LSPClient(SERVER_PATH)
    c.send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}})
    c.recv()

    c.send({'jsonrpc': '2.0', 'id': 2, 'method': 'textDocument/hover', 'params': {
        'textDocument': {'uri': 'file:///test.json-gen-c'},
        'position': {'line': 0, 'character': 0}
    }})
    resp = c.recv()
    # Currently returns null (placeholder), just check it responds
    check('hover returns response', resp is not None and 'result' in resp)
    rc = c.close()
    check('clean shutdown', rc == 0)


def test_unknown_method():
    print('Test: unknown method returns error')
    c = LSPClient(SERVER_PATH)
    c.send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}})
    c.recv()

    c.send({'jsonrpc': '2.0', 'id': 2, 'method': 'nonexistent/method', 'params': {}})
    resp = c.recv()
    check('returns error response', resp is not None and 'error' in resp)
    check('error code is -32601', resp.get('error', {}).get('code') == -32601)
    rc = c.close()
    check('clean shutdown', rc == 0)


def test_didclose_clears():
    print('Test: didClose clears diagnostics')
    c = LSPClient(SERVER_PATH)
    c.send({'jsonrpc': '2.0', 'id': 1, 'method': 'initialize', 'params': {}})
    c.recv()

    # Open with error
    uri = 'file:///close_test.json-gen-c'
    c.send({'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
        'textDocument': {
            'uri': uri,
            'languageId': 'json-gen-c',
            'version': 1,
            'text': 'struct Bad {'
        }
    }})
    c.recv()  # consume error diagnostics

    # Close
    c.send({'jsonrpc': '2.0', 'method': 'textDocument/didClose', 'params': {
        'textDocument': {'uri': uri}
    }})
    diag = c.recv()
    d = diag.get('params', {}).get('diagnostics', [])
    check('diagnostics cleared on close', len(d) == 0)
    rc = c.close()
    check('clean shutdown', rc == 0)


if __name__ == '__main__':
    SERVER_PATH = find_server()
    print(f'Using server: {SERVER_PATH}\n')

    test_initialize()
    test_diagnostics_error()
    test_diagnostics_valid()
    test_diagnostics_update()
    test_completion()
    test_hover()
    test_unknown_method()
    test_didclose_clears()

    print(f'\n{"=" * 40}')
    print(f'Results: {PASS} passed, {FAIL} failed')
    sys.exit(1 if FAIL > 0 else 0)
