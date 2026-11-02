import * as vscode from 'vscode';
import { DiagnosticManager } from './diagnostics';
import { KeetCodeActionProvider } from './codeActions';
import { KeetCompletionItemProvider } from './completions';
import { KeetHoverProvider } from './hover';
import { CommandManager } from './commands';
import { LspClientManager } from './lspClient';

let lspManager: LspClientManager | null = null;
let diagnosticManager: DiagnosticManager | null = null;

export async function activate(context: vscode.ExtensionContext): Promise<void> {
  // 1. Try starting keet-lsp Language Server
  lspManager = new LspClientManager();
  context.subscriptions.push(lspManager);
  const lspStarted = await lspManager.start();

  if (!lspStarted) {
    // Fall back gracefully to standalone compiler checking & providers
    diagnosticManager = new DiagnosticManager();
    context.subscriptions.push(diagnosticManager);

    // Quick Fix Code Action Provider (lightbulb)
    context.subscriptions.push(
      vscode.languages.registerCodeActionsProvider(
        'keetscript',
        new KeetCodeActionProvider(),
        {
          providedCodeActionKinds: KeetCodeActionProvider.providedCodeActionKinds
        }
      )
    );

    // Fallback IntelliSense Autocompletion Provider
    context.subscriptions.push(
      vscode.languages.registerCompletionItemProvider(
        'keetscript',
        new KeetCompletionItemProvider(),
        '.',
        ':',
        '<'
      )
    );

    // Fallback Hover Documentation Provider
    context.subscriptions.push(
      vscode.languages.registerHoverProvider(
        'keetscript',
        new KeetHoverProvider()
      )
    );
  }

  // 2. Commands and Editor UI (Run ▶, Build, Check)
  const commandManager = new CommandManager(diagnosticManager);
  commandManager.registerCommands(context);
}

export async function deactivate(): Promise<void> {
  if (lspManager) {
    await lspManager.stop();
    lspManager = null;
  }
  if (diagnosticManager) {
    diagnosticManager.dispose();
    diagnosticManager = null;
  }
}
