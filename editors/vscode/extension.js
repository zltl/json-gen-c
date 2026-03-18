// extension.js - VS Code extension entry point for json-gen-c language server
'use strict';

const { LanguageClient, TransportKind } = require('vscode-languageclient/node');
const vscode = require('vscode');

let client;

function activate(context) {
    const config = vscode.workspace.getConfiguration('json-gen-c');
    const serverPath = config.get('serverPath', 'json-gen-c');

    const serverOptions = {
        command: serverPath,
        args: ['--lsp'],
        transport: TransportKind.stdio
    };

    const clientOptions = {
        documentSelector: [{ scheme: 'file', language: 'json-gen-c' }],
        synchronize: {
            fileEvents: vscode.workspace.createFileSystemWatcher('**/*.json-gen-c')
        }
    };

    client = new LanguageClient(
        'json-gen-c',
        'json-gen-c Language Server',
        serverOptions,
        clientOptions
    );

    client.start();
}

function deactivate() {
    if (client) {
        return client.stop();
    }
}

module.exports = { activate, deactivate };
