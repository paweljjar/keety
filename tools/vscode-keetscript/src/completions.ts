import * as vscode from 'vscode';
import { STDLIB_MODULES, BUILTIN_METHODS, BUILTIN_TYPES } from './stdlibData';

export class KeetCompletionItemProvider implements vscode.CompletionItemProvider {
  public provideCompletionItems(
    document: vscode.TextDocument,
    position: vscode.Position,
    _token: vscode.CancellationToken,
    _context: vscode.CompletionContext
  ): vscode.CompletionItem[] | vscode.CompletionList {
    const linePrefix = document.lineAt(position).text.substring(0, position.character);

    // 1a. Sub-enum dot completions: e.g. "io.Color."
    if (linePrefix.match(/\bio\.Color\.$/)) {
      const colors = ['Default', 'Red', 'Green', 'Yellow', 'Blue', 'Magenta', 'Cyan', 'White', 'Gray'];
      return colors.map((c) => {
        const item = new vscode.CompletionItem(c, vscode.CompletionItemKind.EnumMember);
        item.detail = `io.Color.${c}`;
        item.documentation = new vscode.MarkdownString(`Terminal color enum member \`io.Color.${c}\`.`);
        return item;
      });
    }

    // 1. Dot completions: e.g. "io.", "fs.", "math.", or "str."
    const dotMatch = linePrefix.match(/([a-zA-Z_][a-zA-Z0-9_]*)\.([a-zA-Z0-9_]*)$/);
    if (dotMatch) {
      const receiver = dotMatch[1];
      if (receiver in STDLIB_MODULES) {
        return this.getStdlibModuleCompletions(receiver);
      }
      // General member method completion
      return this.getBuiltinMethodCompletions();
    }

    // 2. Type completions after ':' or '<' or 'as '
    if (linePrefix.match(/(:\s*|<\s*|as\s+)[a-zA-Z0-9_]*$/)) {
      return this.getTypeCompletions();
    }

    // 3. Top-level keywords, modules, and snippets
    return this.getGeneralCompletions();
  }

  private getStdlibModuleCompletions(moduleName: string): vscode.CompletionItem[] {
    const mod = STDLIB_MODULES[moduleName];
    if (!mod) {
      return [];
    }

    const items: vscode.CompletionItem[] = [];
    for (const [name, member] of Object.entries(mod.members)) {
      const kind = member.kind === 'function'
        ? vscode.CompletionItemKind.Function
        : member.kind === 'constant'
        ? vscode.CompletionItemKind.Constant
        : vscode.CompletionItemKind.Enum;

      const item = new vscode.CompletionItem(name, kind);
      item.detail = member.signature;
      item.documentation = new vscode.MarkdownString(member.doc);

      if (member.snippet) {
        item.insertText = new vscode.SnippetString(member.snippet);
      }
      items.push(item);
    }
    return items;
  }

  private getBuiltinMethodCompletions(): vscode.CompletionItem[] {
    const items: vscode.CompletionItem[] = [];
    for (const [name, method] of Object.entries(BUILTIN_METHODS)) {
      const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Method);
      item.detail = method.signature;
      item.documentation = new vscode.MarkdownString(method.doc);
      if (method.snippet) {
        item.insertText = new vscode.SnippetString(method.snippet);
      }
      items.push(item);
    }
    return items;
  }

  private getTypeCompletions(): vscode.CompletionItem[] {
    const items: vscode.CompletionItem[] = [];
    for (const type of BUILTIN_TYPES) {
      const item = new vscode.CompletionItem(type.name, vscode.CompletionItemKind.TypeParameter);
      item.detail = `type ${type.name}`;
      item.documentation = new vscode.MarkdownString(type.doc);
      items.push(item);
    }

    // Generic types
    const arrayItem = new vscode.CompletionItem('array<T>', vscode.CompletionItemKind.TypeParameter);
    arrayItem.detail = 'array<T>';
    arrayItem.documentation = new vscode.MarkdownString('Generic array container');
    arrayItem.insertText = new vscode.SnippetString('array<${1:int}>');
    items.push(arrayItem);

    const mapItem = new vscode.CompletionItem('map<K, V>', vscode.CompletionItemKind.TypeParameter);
    mapItem.detail = 'map<K, V>';
    mapItem.documentation = new vscode.MarkdownString('Generic key-value associative map');
    mapItem.insertText = new vscode.SnippetString('map<${1:string}, ${2:int}>');
    items.push(mapItem);

    return items;
  }

  private getGeneralCompletions(): vscode.CompletionItem[] {
    const items: vscode.CompletionItem[] = [];

    // Modules
    for (const [name, mod] of Object.entries(STDLIB_MODULES)) {
      const item = new vscode.CompletionItem(name, vscode.CompletionItemKind.Module);
      item.detail = `module ${name}`;
      item.documentation = new vscode.MarkdownString(mod.doc);
      items.push(item);
    }

    // Keywords
    const keywords = [
      { name: 'fn', detail: 'fn <name>(...): <type>', snippet: 'fn ${1:name}(${2:params}): ${3:void} {\n\t$0\n}' },
      { name: 'let', detail: 'let <name>: <type> = <expr>;', snippet: 'let ${1:name}: ${2:type} = ${3:expr};' },
      { name: 'mut', detail: 'mut <name>: <type> = <expr>;', snippet: 'mut ${1:name}: ${2:type} = ${3:expr};' },
      { name: 'const', detail: 'const <name>: <type> = <expr>;', snippet: 'const ${1:NAME}: ${2:type} = ${3:expr};' },
      { name: 'if', detail: 'if (condition) { ... }', snippet: 'if (${1:condition}) {\n\t$0\n}' },
      { name: 'else', detail: 'else { ... }', snippet: 'else {\n\t$0\n}' },
      { name: 'while', detail: 'while (condition) { ... }', snippet: 'while (${1:condition}) {\n\t$0\n}' },
      { name: 'for', detail: 'for (item in collection) { ... }', snippet: 'for (${1:item} in ${2:collection}) {\n\t$0\n}' },
      { name: 'return', detail: 'return <expr>;', snippet: 'return ${1:value};' },
      { name: 'zonk', detail: 'zonk <fallback>', snippet: 'zonk ${1:fallback}' },
      { name: 'class', detail: 'class <Name> { ... }', snippet: 'class ${1:Name} {\n\t$0\n}' },
      { name: 'enum', detail: 'enum <Name> { ... }', snippet: 'enum ${1:Name} {\n\t$0\n}' },
      { name: 'import', detail: 'import <module>;', snippet: 'import ${1:io};' }
    ];

    for (const kw of keywords) {
      const item = new vscode.CompletionItem(kw.name, vscode.CompletionItemKind.Keyword);
      item.detail = kw.detail;
      item.insertText = new vscode.SnippetString(kw.snippet);
      items.push(item);
    }

    return items;
  }
}
