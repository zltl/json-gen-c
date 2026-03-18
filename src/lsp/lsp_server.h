/*
 * lsp_server.h - LSP server for json-gen-c schema files
 */

#ifndef LSP_SERVER_H
#define LSP_SERVER_H

/**
 * @brief Run the LSP server main loop.
 * Reads JSON-RPC messages from stdin, writes responses to stdout.
 * Returns 0 on clean shutdown, non-zero on error.
 */
int lsp_server_run(void);

#endif /* LSP_SERVER_H */
