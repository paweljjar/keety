import * as vscode from 'vscode';
import { STDLIB_MODULES, BUILTIN_METHODS, BUILTIN_TYPES } from './stdlibData';

export class KeetHoverProvider implements vscode.HoverProvider {
  public provideHover(
    document: vscode.TextDocument,
    position: vscode.Position,
    _token: vscode.CancellationToken
  ): vscode.Hover | null {
    const range = document.getWordRangeAtPosition(position);
    if (!range) {
      return null;
    }

    const word = document.getText(range);
    const linePrefix = document.lineAt(position).text.substring(0, range.start.character);

    // 1. Check if preceded by a dot: e.g. "io.println", "fs.readText", "math.PI"
    const dotMatch = linePrefix.match(/([a-zA-Z_][a-zA-Z0-9_]*)\.$/);
    if (dotMatch) {
      const receiver = dotMatch[1];
      if (receiver in STDLIB_MODULES) {
        const member = STDLIB_MODULES[receiver].members[word];
        if (member) {
          const content = new vscode.MarkdownString();
          content.appendCodeblock(member.signature, 'keetscript');
          content.appendMarkdown(`\n\n${member.doc}\n\n*Part of standard library module \`${receiver}\`*`);
          return new vscode.Hover(content, range);
        }
      }

      // Built-in method
      const method = BUILTIN_METHODS[word];
      if (method) {
        const content = new vscode.MarkdownString();
        content.appendCodeblock(method.signature, 'keetscript');
        content.appendMarkdown(`\n\n${method.doc}`);
        return new vscode.Hover(content, range);
      }
    }

    // 2. Standard library module name hover
    if (word in STDLIB_MODULES) {
      const mod = STDLIB_MODULES[word];
      const content = new vscode.MarkdownString();
      content.appendCodeblock(`module ${mod.name}`, 'keetscript');
      content.appendMarkdown(`\n\n${mod.doc}\n\n**Exported members:** ` + Object.keys(mod.members).map(m => `\`${m}\``).join(', '));
      return new vscode.Hover(content, range);
    }

    // 3. Built-in method hover directly on identifier
    if (word in BUILTIN_METHODS) {
      const method = BUILTIN_METHODS[word];
      const content = new vscode.MarkdownString();
      content.appendCodeblock(method.signature, 'keetscript');
      content.appendMarkdown(`\n\n${method.doc}`);
      return new vscode.Hover(content, range);
    }

    // 4. Built-in types hover
    const typeInfo = BUILTIN_TYPES.find(t => t.name === word);
    if (typeInfo) {
      const content = new vscode.MarkdownString();
      content.appendCodeblock(`type ${typeInfo.name}`, 'keetscript');
      content.appendMarkdown(`\n\n${typeInfo.doc}`);
      return new vscode.Hover(content, range);
    }

    // 5. Special KeetScript keywords
    const keywordDocs: Record<string, { sig: string; desc: string }> = {
      zonk: {
        sig: 'expr zonk fallback\nexpr zonk (err) { yield fallback; }',
        desc: 'Error recovery operator. Catches errors produced by `expr` and evaluates fallback expression or recovery block.'
      },
      yield: {
        sig: 'yield value;',
        desc: 'Yields a recovery value from inside a `zonk (err) { ... }` recovery block.'
      },
      fn: {
        sig: 'fn name(params): ReturnType { ... }',
        desc: 'Declares a function with typed parameters and return type annotation.'
      },
      let: {
        sig: 'let name: Type = expr;',
        desc: 'Declares an immutable local variable.'
      },
      mut: {
        sig: 'mut name: Type = expr;',
        desc: 'Declares a mutable local variable.'
      },
      const: {
        sig: 'const NAME: Type = expr;',
        desc: 'Declares a compile-time constant.'
      }
    };

    if (word in keywordDocs) {
      const kw = keywordDocs[word];
      const content = new vscode.MarkdownString();
      content.appendCodeblock(kw.sig, 'keetscript');
      content.appendMarkdown(`\n\n${kw.desc}`);
      return new vscode.Hover(content, range);
    }

    return null;
  }
}
