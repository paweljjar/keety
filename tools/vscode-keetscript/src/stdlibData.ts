export interface StdlibMember {
  name: string;
  kind: 'function' | 'constant' | 'enum' | string;
  signature: string;
  doc: string;
  snippet?: string;
}

export interface StdlibModule {
  name: string;
  doc: string;
  members: Record<string, StdlibMember>;
}

export const STDLIB_MODULES: Record<string, StdlibModule> = {
  io: {
    name: 'io',
    doc: 'Input and output operations, terminal formatting, and standard streams.',
    members: {
      print: {
        name: 'print',
        kind: 'function',
        signature: 'fn print(...args: string | int | float | bool | char | null, color?: io.Color): void',
        doc: 'Prints arguments to standard output without a trailing newline.',
        snippet: 'print(${1:args})'
      },
      println: {
        name: 'println',
        kind: 'function',
        signature: 'fn println(...args: string | int | float | bool | char | null, color?: io.Color): void',
        doc: 'Prints arguments to standard output followed by a newline.',
        snippet: 'println(${1:args})'
      },
      eprint: {
        name: 'eprint',
        kind: 'function',
        signature: 'fn eprint(...args: string | int | float | bool | char | null, color?: io.Color): void',
        doc: 'Prints arguments to standard error without a trailing newline.',
        snippet: 'eprint(${1:args})'
      },
      eprintln: {
        name: 'eprintln',
        kind: 'function',
        signature: 'fn eprintln(...args: string | int | float | bool | char | null, color?: io.Color): void',
        doc: 'Prints arguments to standard error followed by a newline.',
        snippet: 'eprintln(${1:args})'
      },
      readln: {
        name: 'readln',
        kind: 'function',
        signature: 'fn readln(): string',
        doc: 'Reads a line of text from standard input (without newline).',
        snippet: 'readln()'
      },
      readLine: {
        name: 'readLine',
        kind: 'function',
        signature: 'fn readLine(prompt?: string): string',
        doc: 'Reads a line of text from standard input (without newline), with an optional prompt.',
        snippet: 'readLine()'
      },
      input: {
        name: 'input',
        kind: 'function',
        signature: 'fn input(prompt?: string): string',
        doc: 'Displays an optional prompt and reads a line of user input.',
        snippet: 'input("${1:prompt: }")'
      },
      eof: {
        name: 'eof',
        kind: 'function',
        signature: 'fn eof(): bool',
        doc: 'Returns true if standard input has reached end-of-file.',
        snippet: 'eof()'
      },
      Color: {
        name: 'Color',
        kind: 'enum',
        signature: 'enum Color { Default, Red, Green, Yellow, Blue, Magenta, Cyan, White, Gray }',
        doc: 'Terminal text color enum for io.print, io.println, io.eprint, and io.eprintln.'
      },
      flush: {
        name: 'flush',
        kind: 'function',
        signature: 'fn flush(): void',
        doc: 'Flushes standard output buffer.',
        snippet: 'flush()'
      },
      red: {
        name: 'red',
        kind: 'function',
        signature: 'fn red(text: string): string',
        doc: 'Wraps text in ANSI red escape code.',
        snippet: 'red(${1:text})'
      },
      green: {
        name: 'green',
        kind: 'function',
        signature: 'fn green(text: string): string',
        doc: 'Wraps text in ANSI green escape code.',
        snippet: 'green(${1:text})'
      },
      yellow: {
        name: 'yellow',
        kind: 'function',
        signature: 'fn yellow(text: string): string',
        doc: 'Wraps text in ANSI yellow escape code.',
        snippet: 'yellow(${1:text})'
      },
      blue: {
        name: 'blue',
        kind: 'function',
        signature: 'fn blue(text: string): string',
        doc: 'Wraps text in ANSI blue escape code.',
        snippet: 'blue(${1:text})'
      },
      magenta: {
        name: 'magenta',
        kind: 'function',
        signature: 'fn magenta(text: string): string',
        doc: 'Wraps text in ANSI magenta escape code.',
        snippet: 'magenta(${1:text})'
      },
      cyan: {
        name: 'cyan',
        kind: 'function',
        signature: 'fn cyan(text: string): string',
        doc: 'Wraps text in ANSI cyan escape code.',
        snippet: 'cyan(${1:text})'
      },
      white: {
        name: 'white',
        kind: 'function',
        signature: 'fn white(text: string): string',
        doc: 'Wraps text in ANSI white escape code.',
        snippet: 'white(${1:text})'
      },
      gray: {
        name: 'gray',
        kind: 'function',
        signature: 'fn gray(text: string): string',
        doc: 'Wraps text in ANSI gray escape code.',
        snippet: 'gray(${1:text})'
      },
      bold: {
        name: 'bold',
        kind: 'function',
        signature: 'fn bold(text: string): string',
        doc: 'Wraps text in ANSI bold escape code.',
        snippet: 'bold(${1:text})'
      }
    }
  },
  fs: {
    name: 'fs',
    doc: 'File system operations (reading, writing, paths, and directory inspection).',
    members: {
      readText: {
        name: 'readText',
        kind: 'function',
        signature: 'fn readText(path: string): string',
        doc: 'Reads the entire contents of a file as a UTF-8 string.',
        snippet: 'readText(${1:path})'
      },
      readLines: {
        name: 'readLines',
        kind: 'function',
        signature: 'fn readLines(path: string): array<string>',
        doc: 'Reads a file and returns an array of lines.',
        snippet: 'readLines(${1:path})'
      },
      writeText: {
        name: 'writeText',
        kind: 'function',
        signature: 'fn writeText(path: string, content: string): void',
        doc: 'Writes text to a file, replacing existing content.',
        snippet: 'writeText(${1:path}, ${2:content})'
      },
      appendText: {
        name: 'appendText',
        kind: 'function',
        signature: 'fn appendText(path: string, content: string): void',
        doc: 'Appends text to the end of a file.',
        snippet: 'appendText(${1:path}, ${2:content})'
      },
      exists: {
        name: 'exists',
        kind: 'function',
        signature: 'fn exists(path: string): bool',
        doc: 'Returns true if the given file or directory path exists.',
        snippet: 'exists(${1:path})'
      },
      isFile: {
        name: 'isFile',
        kind: 'function',
        signature: 'fn isFile(path: string): bool',
        doc: 'Returns true if the given path is a regular file.',
        snippet: 'isFile(${1:path})'
      },
      isDir: {
        name: 'isDir',
        kind: 'function',
        signature: 'fn isDir(path: string): bool',
        doc: 'Returns true if the given path is a directory.',
        snippet: 'isDir(${1:path})'
      },
      fileSize: {
        name: 'fileSize',
        kind: 'function',
        signature: 'fn fileSize(path: string): int',
        doc: 'Returns the file size in bytes.',
        snippet: 'fileSize(${1:path})'
      },
      mkdir: {
        name: 'mkdir',
        kind: 'function',
        signature: 'fn mkdir(path: string): void',
        doc: 'Creates a directory (including parent directories if needed).',
        snippet: 'mkdir(${1:path})'
      },
      remove: {
        name: 'remove',
        kind: 'function',
        signature: 'fn remove(path: string): void',
        doc: 'Removes a file or empty directory.',
        snippet: 'remove(${1:path})'
      },
      removeAll: {
        name: 'removeAll',
        kind: 'function',
        signature: 'fn removeAll(path: string): void',
        doc: 'Recursively removes a directory and all its contents.',
        snippet: 'removeAll(${1:path})'
      },
      copy: {
        name: 'copy',
        kind: 'function',
        signature: 'fn copy(source: string, destination: string): void',
        doc: 'Copies a file from source to destination path.',
        snippet: 'copy(${1:source}, ${2:destination})'
      },
      rename: {
        name: 'rename',
        kind: 'function',
        signature: 'fn rename(oldPath: string, newPath: string): void',
        doc: 'Renames or moves a file or directory.',
        snippet: 'rename(${1:oldPath}, ${2:newPath})'
      },
      listDir: {
        name: 'listDir',
        kind: 'function',
        signature: 'fn listDir(path: string): array<string>',
        doc: 'Lists the names of entries in the directory.',
        snippet: 'listDir(${1:path})'
      },
      cwd: {
        name: 'cwd',
        kind: 'function',
        signature: 'fn cwd(): string',
        doc: 'Returns the current working directory path.',
        snippet: 'cwd()'
      },
      join: {
        name: 'join',
        kind: 'function',
        signature: 'fn join(path1: string, path2: string): string',
        doc: 'Joins two file path components with standard path separator.',
        snippet: 'join(${1:path1}, ${2:path2})'
      },
      fileName: {
        name: 'fileName',
        kind: 'function',
        signature: 'fn fileName(path: string): string',
        doc: 'Extracts the file name component from a path.',
        snippet: 'fileName(${1:path})'
      },
      extension: {
        name: 'extension',
        kind: 'function',
        signature: 'fn extension(path: string): string',
        doc: 'Extracts the file extension from a path.',
        snippet: 'extension(${1:path})'
      },
      parentDir: {
        name: 'parentDir',
        kind: 'function',
        signature: 'fn parentDir(path: string): string',
        doc: 'Extracts the parent directory path.',
        snippet: 'parentDir(${1:path})'
      }
    }
  },
  sys: {
    name: 'sys',
    doc: 'System environment, processes, arguments, and platform inspection.',
    members: {
      args: {
        name: 'args',
        kind: 'function',
        signature: 'fn args(): array<string>',
        doc: 'Returns the command line arguments passed to the program.',
        snippet: 'args()'
      },
      exit: {
        name: 'exit',
        kind: 'function',
        signature: 'fn exit(code: int): void',
        doc: 'Exits the process immediately with the given exit code.',
        snippet: 'exit(${1:0})'
      },
      pid: {
        name: 'pid',
        kind: 'function',
        signature: 'fn pid(): int',
        doc: 'Returns the process ID of the current process.',
        snippet: 'pid()'
      },
      getEnv: {
        name: 'getEnv',
        kind: 'function',
        signature: 'fn getEnv(name: string): string | null',
        doc: 'Returns the value of an environment variable, or null if not set.',
        snippet: 'getEnv("${1:KEY}")'
      },
      getEnvOr: {
        name: 'getEnvOr',
        kind: 'function',
        signature: 'fn getEnvOr(name: string, defaultValue: string): string',
        doc: 'Returns the environment variable value or fallback defaultValue.',
        snippet: 'getEnvOr("${1:KEY}", "${2:default}")'
      },
      setEnv: {
        name: 'setEnv',
        kind: 'function',
        signature: 'fn setEnv(name: string, value: string): void',
        doc: 'Sets the value of an environment variable.',
        snippet: 'setEnv("${1:KEY}", ${2:value})'
      },
      unsetEnv: {
        name: 'unsetEnv',
        kind: 'function',
        signature: 'fn unsetEnv(name: string): void',
        doc: 'Unsets an environment variable.',
        snippet: 'unsetEnv("${1:KEY}")'
      },
      hasEnv: {
        name: 'hasEnv',
        kind: 'function',
        signature: 'fn hasEnv(name: string): bool',
        doc: 'Returns true if the environment variable is defined.',
        snippet: 'hasEnv("${1:KEY}")'
      },
      platform: {
        name: 'platform',
        kind: 'function',
        signature: 'fn platform(): string',
        doc: 'Returns host OS name (e.g. "macos", "linux", "windows").',
        snippet: 'platform()'
      },
      arch: {
        name: 'arch',
        kind: 'function',
        signature: 'fn arch(): string',
        doc: 'Returns host CPU architecture (e.g. "arm64", "x86_64").',
        snippet: 'arch()'
      },
      cpuCount: {
        name: 'cpuCount',
        kind: 'function',
        signature: 'fn cpuCount(): int',
        doc: 'Returns the number of logical CPU cores.',
        snippet: 'cpuCount()'
      },
      cwd: {
        name: 'cwd',
        kind: 'function',
        signature: 'fn cwd(): string',
        doc: 'Returns the current working directory.',
        snippet: 'cwd()'
      },
      chdir: {
        name: 'chdir',
        kind: 'function',
        signature: 'fn chdir(path: string): void',
        doc: 'Changes the current working directory.',
        snippet: 'chdir(${1:path})'
      },
      homeDir: {
        name: 'homeDir',
        kind: 'function',
        signature: 'fn homeDir(): string',
        doc: 'Returns the user home directory path.',
        snippet: 'homeDir()'
      },
      tempDir: {
        name: 'tempDir',
        kind: 'function',
        signature: 'fn tempDir(): string',
        doc: 'Returns the system temporary files directory.',
        snippet: 'tempDir()'
      },
      exec: {
        name: 'exec',
        kind: 'function',
        signature: 'fn exec(command: string): int',
        doc: 'Executes a shell command and returns its exit code.',
        snippet: 'exec(${1:command})'
      },
      execOutput: {
        name: 'execOutput',
        kind: 'function',
        signature: 'fn execOutput(command: string): string',
        doc: 'Executes a command and captures its standard output as a string.',
        snippet: 'execOutput(${1:command})'
      }
    }
  },
  math: {
    name: 'math',
    doc: 'Mathematical constants, trigonometry, powers, rounding, and utilities.',
    members: {
      PI: {
        name: 'PI',
        kind: 'constant',
        signature: 'const PI: float = 3.141592653589793',
        doc: 'The ratio of a circle\'s circumference to its diameter.'
      },
      E: {
        name: 'E',
        kind: 'constant',
        signature: 'const E: float = 2.718281828459045',
        doc: 'Euler\'s number, base of the natural logarithm.'
      },
      TAU: {
        name: 'TAU',
        kind: 'constant',
        signature: 'const TAU: float = 6.283185307179586',
        doc: 'Ratio of circumference to radius (2 * PI).'
      },
      INF: {
        name: 'INF',
        kind: 'constant',
        signature: 'const INF: float',
        doc: 'Positive infinity float value.'
      },
      NAN: {
        name: 'NAN',
        kind: 'constant',
        signature: 'const NAN: float',
        doc: 'Not-a-Number float value.'
      },
      abs: {
        name: 'abs',
        kind: 'function',
        signature: 'fn abs(x: float): float',
        doc: 'Returns the absolute value of x.',
        snippet: 'abs(${1:x})'
      },
      min: {
        name: 'min',
        kind: 'function',
        signature: 'fn min(a: float, b: float): float',
        doc: 'Returns the smaller of two numbers.',
        snippet: 'min(${1:a}, ${2:b})'
      },
      max: {
        name: 'max',
        kind: 'function',
        signature: 'fn max(a: float, b: float): float',
        doc: 'Returns the larger of two numbers.',
        snippet: 'max(${1:a}, ${2:b})'
      },
      clamp: {
        name: 'clamp',
        kind: 'function',
        signature: 'fn clamp(val: float, min: float, max: float): float',
        doc: 'Clamps val between min and max bounds.',
        snippet: 'clamp(${1:val}, ${2:min}, ${3:max})'
      },
      floor: {
        name: 'floor',
        kind: 'function',
        signature: 'fn floor(x: float): int',
        doc: 'Rounds down to nearest integer.',
        snippet: 'floor(${1:x})'
      },
      ceil: {
        name: 'ceil',
        kind: 'function',
        signature: 'fn ceil(x: float): int',
        doc: 'Rounds up to nearest integer.',
        snippet: 'ceil(${1:x})'
      },
      round: {
        name: 'round',
        kind: 'function',
        signature: 'fn round(x: float): int',
        doc: 'Rounds to nearest integer.',
        snippet: 'round(${1:x})'
      },
      trunc: {
        name: 'trunc',
        kind: 'function',
        signature: 'fn trunc(x: float): int',
        doc: 'Truncates fractional digits toward zero.',
        snippet: 'trunc(${1:x})'
      },
      sqrt: {
        name: 'sqrt',
        kind: 'function',
        signature: 'fn sqrt(x: float): float',
        doc: 'Returns the square root of x.',
        snippet: 'sqrt(${1:x})'
      },
      cbrt: {
        name: 'cbrt',
        kind: 'function',
        signature: 'fn cbrt(x: float): float',
        doc: 'Returns the cube root of x.',
        snippet: 'cbrt(${1:x})'
      },
      pow: {
        name: 'pow',
        kind: 'function',
        signature: 'fn pow(base: float, exponent: float): float',
        doc: 'Returns base raised to power exponent.',
        snippet: 'pow(${1:base}, ${2:exponent})'
      },
      hypot: {
        name: 'hypot',
        kind: 'function',
        signature: 'fn hypot(x: float, y: float): float',
        doc: 'Returns hypotenuse sqrt(x*x + y*y).',
        snippet: 'hypot(${1:x}, ${2:y})'
      },
      exp: {
        name: 'exp',
        kind: 'function',
        signature: 'fn exp(x: float): float',
        doc: 'Returns e raised to power x.',
        snippet: 'exp(${1:x})'
      },
      log: {
        name: 'log',
        kind: 'function',
        signature: 'fn log(x: float): float',
        doc: 'Returns the natural logarithm (base e) of x.',
        snippet: 'log(${1:x})'
      },
      log10: {
        name: 'log10',
        kind: 'function',
        signature: 'fn log10(x: float): float',
        doc: 'Returns the base-10 logarithm of x.',
        snippet: 'log10(${1:x})'
      },
      log2: {
        name: 'log2',
        kind: 'function',
        signature: 'fn log2(x: float): float',
        doc: 'Returns the binary (base-2) logarithm of x.',
        snippet: 'log2(${1:x})'
      },
      sin: {
        name: 'sin',
        kind: 'function',
        signature: 'fn sin(rad: float): float',
        doc: 'Returns sine of angle in radians.',
        snippet: 'sin(${1:rad})'
      },
      cos: {
        name: 'cos',
        kind: 'function',
        signature: 'fn cos(rad: float): float',
        doc: 'Returns cosine of angle in radians.',
        snippet: 'cos(${1:rad})'
      },
      tan: {
        name: 'tan',
        kind: 'function',
        signature: 'fn tan(rad: float): float',
        doc: 'Returns tangent of angle in radians.',
        snippet: 'tan(${1:rad})'
      },
      asin: {
        name: 'asin',
        kind: 'function',
        signature: 'fn asin(x: float): float',
        doc: 'Returns arcsine in radians.',
        snippet: 'asin(${1:x})'
      },
      acos: {
        name: 'acos',
        kind: 'function',
        signature: 'fn acos(x: float): float',
        doc: 'Returns arccosine in radians.',
        snippet: 'acos(${1:x})'
      },
      atan: {
        name: 'atan',
        kind: 'function',
        signature: 'fn atan(x: float): float',
        doc: 'Returns arctangent in radians.',
        snippet: 'atan(${1:x})'
      },
      atan2: {
        name: 'atan2',
        kind: 'function',
        signature: 'fn atan2(y: float, x: float): float',
        doc: 'Returns angle in radians between positive x-axis and point (x, y).',
        snippet: 'atan2(${1:y}, ${2:x})'
      },
      random: {
        name: 'random',
        kind: 'function',
        signature: 'fn random(): float',
        doc: 'Generates a random pseudo-uniform float in [0.0, 1.0).',
        snippet: 'random()'
      }
    }
  },
  time: {
    name: 'time',
    doc: 'Time measurement, clocks, sleep timers, and ISO-8601 formatting.',
    members: {
      MS_PER_SEC: {
        name: 'MS_PER_SEC',
        kind: 'constant',
        signature: 'const MS_PER_SEC: int = 1000',
        doc: 'Number of milliseconds in one second.'
      },
      NS_PER_SEC: {
        name: 'NS_PER_SEC',
        kind: 'constant',
        signature: 'const NS_PER_SEC: int = 1000000000',
        doc: 'Number of nanoseconds in one second.'
      },
      SEC_PER_MIN: {
        name: 'SEC_PER_MIN',
        kind: 'constant',
        signature: 'const SEC_PER_MIN: int = 60',
        doc: 'Number of seconds in one minute.'
      },
      MIN_PER_HOUR: {
        name: 'MIN_PER_HOUR',
        kind: 'constant',
        signature: 'const MIN_PER_HOUR: int = 60',
        doc: 'Number of minutes in one hour.'
      },
      now: {
        name: 'now',
        kind: 'function',
        signature: 'fn now(): int',
        doc: 'Returns current UNIX epoch timestamp in seconds.',
        snippet: 'now()'
      },
      nowSec: {
        name: 'nowSec',
        kind: 'function',
        signature: 'fn nowSec(): int',
        doc: 'Returns current UNIX epoch timestamp in seconds.',
        snippet: 'nowSec()'
      },
      nowMs: {
        name: 'nowMs',
        kind: 'function',
        signature: 'fn nowMs(): int',
        doc: 'Returns current UNIX epoch timestamp in milliseconds.',
        snippet: 'nowMs()'
      },
      monotonicMs: {
        name: 'monotonicMs',
        kind: 'function',
        signature: 'fn monotonicMs(): int',
        doc: 'Returns steady monotonic time in milliseconds (immune to clock adjustments).',
        snippet: 'monotonicMs()'
      },
      elapsedMs: {
        name: 'elapsedMs',
        kind: 'function',
        signature: 'fn elapsedMs(startMs: int): int',
        doc: 'Calculates elapsed milliseconds since startMs.',
        snippet: 'elapsedMs(${1:startMs})'
      },
      sleep: {
        name: 'sleep',
        kind: 'function',
        signature: 'fn sleep(ms: int): void',
        doc: 'Suspends execution for the specified milliseconds.',
        snippet: 'sleep(${1:ms})'
      },
      formatIso8601: {
        name: 'formatIso8601',
        kind: 'function',
        signature: 'fn formatIso8601(timestampSec: int): string',
        doc: 'Formats a UNIX timestamp into UTC ISO-8601 string ("YYYY-MM-DDTHH:MM:SSZ").',
        snippet: 'formatIso8601(${1:timestampSec})'
      },
      formatLocal: {
        name: 'formatLocal',
        kind: 'function',
        signature: 'fn formatLocal(timestampSec: int, format: string): string',
        doc: 'Formats timestamp with local strftime format string.',
        snippet: 'formatLocal(${1:timestampSec}, "${2:%Y-%m-%d %H:%M:%S}")'
      }
    }
  },
  json: {
    name: 'json',
    doc: 'RFC 8259 JSON validation, dot-path extraction, collection parsers, immutable builders, and polymorphic serialization.',
    members: {
      isValid: {
        name: 'isValid',
        kind: 'function',
        signature: 'fn isValid(text: string): bool',
        doc: 'Returns true if text is strictly valid RFC 8259 JSON.',
        snippet: 'isValid(${1:text})'
      },
      typeOf: {
        name: 'typeOf',
        kind: 'function',
        signature: 'fn typeOf(text: string): string',
        doc: 'Returns "object", "array", "string", "number", "bool", "null", or "invalid".',
        snippet: 'typeOf(${1:text})'
      },
      isObject: {
        name: 'isObject',
        kind: 'function',
        signature: 'fn isObject(text: string): bool',
        doc: 'Returns true if text is a valid JSON object ({...}).',
        snippet: 'isObject(${1:text})'
      },
      isArray: {
        name: 'isArray',
        kind: 'function',
        signature: 'fn isArray(text: string): bool',
        doc: 'Returns true if text is a valid JSON array ([...]).',
        snippet: 'isArray(${1:text})'
      },
      size: {
        name: 'size',
        kind: 'function',
        signature: 'fn size(text: string): int',
        doc: 'Returns the entry/element count of a JSON object or array.',
        snippet: 'size(${1:text})'
      },
      keys: {
        name: 'keys',
        kind: 'function',
        signature: 'fn keys(text: string): array<string>',
        doc: 'Returns the sorted top-level keys of a JSON object.',
        snippet: 'keys(${1:text})'
      },
      hasKey: {
        name: 'hasKey',
        kind: 'function',
        signature: 'fn hasKey(text: string, path: string): bool',
        doc: 'Returns true if path resolves to an existing key or array index.',
        snippet: 'hasKey(${1:text}, "${2:path}")'
      },
      isNull: {
        name: 'isNull',
        kind: 'function',
        signature: 'fn isNull(text: string, path: string): bool',
        doc: 'Returns true if path resolves to a JSON null literal.',
        snippet: 'isNull(${1:text}, "${2:path}")'
      },
      getString: {
        name: 'getString',
        kind: 'function',
        signature: 'fn getString(text: string, path: string): string',
        doc: 'Extracts the unescaped string at dot-path (throws JsonError on failure).',
        snippet: 'getString(${1:text}, "${2:path}")'
      },
      getInt: {
        name: 'getInt',
        kind: 'function',
        signature: 'fn getInt(text: string, path: string): int',
        doc: 'Extracts an exact 64-bit integer at dot-path (throws JsonError on failure).',
        snippet: 'getInt(${1:text}, "${2:path}")'
      },
      getFloat: {
        name: 'getFloat',
        kind: 'function',
        signature: 'fn getFloat(text: string, path: string): float',
        doc: 'Extracts a float at dot-path (throws JsonError on failure).',
        snippet: 'getFloat(${1:text}, "${2:path}")'
      },
      getBool: {
        name: 'getBool',
        kind: 'function',
        signature: 'fn getBool(text: string, path: string): bool',
        doc: 'Extracts a boolean at dot-path (throws JsonError on failure).',
        snippet: 'getBool(${1:text}, "${2:path}")'
      },
      getArray: {
        name: 'getArray',
        kind: 'function',
        signature: 'fn getArray(text: string, path: string): array<string>',
        doc: 'Extracts a JSON array at dot-path as array<string>.',
        snippet: 'getArray(${1:text}, "${2:path}")'
      },
      getObject: {
        name: 'getObject',
        kind: 'function',
        signature: 'fn getObject(text: string, path: string): string',
        doc: 'Extracts a nested JSON object at dot-path as a serialized JSON string.',
        snippet: 'getObject(${1:text}, "${2:path}")'
      },
      getStringOr: {
        name: 'getStringOr',
        kind: 'function',
        signature: 'fn getStringOr(text: string, path: string, defaultVal: string): string',
        doc: 'Returns getString(text, path) on success, or defaultVal on any error.',
        snippet: 'getStringOr(${1:text}, "${2:path}", ${3:defaultVal})'
      },
      getIntOr: {
        name: 'getIntOr',
        kind: 'function',
        signature: 'fn getIntOr(text: string, path: string, defaultVal: int): int',
        doc: 'Returns getInt(text, path) on success, or defaultVal on any error.',
        snippet: 'getIntOr(${1:text}, "${2:path}", ${3:0})'
      },
      getFloatOr: {
        name: 'getFloatOr',
        kind: 'function',
        signature: 'fn getFloatOr(text: string, path: string, defaultVal: float): float',
        doc: 'Returns getFloat(text, path) on success, or defaultVal on any error.',
        snippet: 'getFloatOr(${1:text}, "${2:path}", ${3:0.0})'
      },
      getBoolOr: {
        name: 'getBoolOr',
        kind: 'function',
        signature: 'fn getBoolOr(text: string, path: string, defaultVal: bool): bool',
        doc: 'Returns getBool(text, path) on success, or defaultVal on any error.',
        snippet: 'getBoolOr(${1:text}, "${2:path}", ${3:false})'
      },
      parseMap: {
        name: 'parseMap',
        kind: 'function',
        signature: 'fn parseMap(text: string): map<string, string>',
        doc: 'Parses a JSON object into a map<string, string>.',
        snippet: 'parseMap(${1:text})'
      },
      parseArray: {
        name: 'parseArray',
        kind: 'function',
        signature: 'fn parseArray(text: string): array<string>',
        doc: 'Parses a JSON array into an array<string>.',
        snippet: 'parseArray(${1:text})'
      },
      parseIntArray: {
        name: 'parseIntArray',
        kind: 'function',
        signature: 'fn parseIntArray(text: string): array<int>',
        doc: 'Parses a homogeneous JSON array of integers into array<int>.',
        snippet: 'parseIntArray(${1:text})'
      },
      parseFloatArray: {
        name: 'parseFloatArray',
        kind: 'function',
        signature: 'fn parseFloatArray(text: string): array<float>',
        doc: 'Parses a homogeneous JSON array of numbers into array<float>.',
        snippet: 'parseFloatArray(${1:text})'
      },
      parseBoolArray: {
        name: 'parseBoolArray',
        kind: 'function',
        signature: 'fn parseBoolArray(text: string): array<bool>',
        doc: 'Parses a homogeneous JSON array of booleans into array<bool>.',
        snippet: 'parseBoolArray(${1:text})'
      },
      emptyObject: {
        name: 'emptyObject',
        kind: 'function',
        signature: 'fn emptyObject(): string',
        doc: 'Returns an empty JSON object literal "{}".',
        snippet: 'emptyObject()'
      },
      emptyArray: {
        name: 'emptyArray',
        kind: 'function',
        signature: 'fn emptyArray(): string',
        doc: 'Returns an empty JSON array literal "[]".',
        snippet: 'emptyArray()'
      },
      setString: {
        name: 'setString',
        kind: 'function',
        signature: 'fn setString(text: string, path: string, value: string): string',
        doc: 'Sets dot-path to a string value and returns the updated JSON string.',
        snippet: 'setString(${1:text}, "${2:path}", ${3:value})'
      },
      setInt: {
        name: 'setInt',
        kind: 'function',
        signature: 'fn setInt(text: string, path: string, value: int): string',
        doc: 'Sets dot-path to an integer value and returns the updated JSON string.',
        snippet: 'setInt(${1:text}, "${2:path}", ${3:value})'
      },
      setFloat: {
        name: 'setFloat',
        kind: 'function',
        signature: 'fn setFloat(text: string, path: string, value: float): string',
        doc: 'Sets dot-path to a finite float value and returns the updated JSON string.',
        snippet: 'setFloat(${1:text}, "${2:path}", ${3:value})'
      },
      setBool: {
        name: 'setBool',
        kind: 'function',
        signature: 'fn setBool(text: string, path: string, value: bool): string',
        doc: 'Sets dot-path to a boolean value and returns the updated JSON string.',
        snippet: 'setBool(${1:text}, "${2:path}", ${3:value})'
      },
      setNull: {
        name: 'setNull',
        kind: 'function',
        signature: 'fn setNull(text: string, path: string): string',
        doc: 'Sets dot-path to null and returns the updated JSON string.',
        snippet: 'setNull(${1:text}, "${2:path}")'
      },
      setRaw: {
        name: 'setRaw',
        kind: 'function',
        signature: 'fn setRaw(text: string, path: string, rawJson: string): string',
        doc: 'Validates rawJson and embeds it at dot-path inside text.',
        snippet: 'setRaw(${1:text}, "${2:path}", ${3:rawJson})'
      },
      removeKey: {
        name: 'removeKey',
        kind: 'function',
        signature: 'fn removeKey(text: string, path: string): string',
        doc: 'Removes the object key or array index at dot-path and returns the updated JSON string.',
        snippet: 'removeKey(${1:text}, "${2:path}")'
      },
      stringify: {
        name: 'stringify',
        kind: 'function',
        signature: 'fn stringify(value: Any): string',
        doc: 'Serializes any JSON-compatible native KeetScript value to an RFC 8259 JSON string.',
        snippet: 'stringify(${1:value})'
      },
      format: {
        name: 'format',
        kind: 'function',
        signature: 'fn format(text: string, indent: int): string',
        doc: 'Validates text and pretty-prints it with indent spaces per nesting level (0..16).',
        snippet: 'format(${1:text}, ${2:2})'
      },
      minify: {
        name: 'minify',
        kind: 'function',
        signature: 'fn minify(text: string): string',
        doc: 'Validates text and removes all insignificant whitespace.',
        snippet: 'minify(${1:text})'
      },
      escape: {
        name: 'escape',
        kind: 'function',
        signature: 'fn escape(str: string): string',
        doc: 'Escapes a raw string into a double-quoted JSON string literal ("...").',
        snippet: 'escape(${1:str})'
      },
      unescape: {
        name: 'unescape',
        kind: 'function',
        signature: 'fn unescape(str: string): string',
        doc: 'Unescapes a JSON string literal.',
        snippet: 'unescape(${1:str})'
      },
      readFile: {
        name: 'readFile',
        kind: 'function',
        signature: 'fn readFile(path: string): string',
        doc: 'Reads UTF-8 file at path, validates its JSON content, and returns the canonical JSON string.',
        snippet: 'readFile(${1:path})'
      },
      writeFile: {
        name: 'writeFile',
        kind: 'function',
        signature: 'fn writeFile(path: string, text: string, indent: int): bool',
        doc: 'Validates text, formats with indent spaces, and writes to path.',
        snippet: 'writeFile(${1:path}, ${2:text}, ${3:2})'
      }
    }
  },
  net: {
    name: 'net',
    doc: 'Networking suite: RFC 3986 URL & query parsing, DNS/IP utilities, HTTP/1.1 & HTTPS client, and handle-based TCP/UDP sockets.',
    members: {
      urlEncode: {
        name: 'urlEncode',
        kind: 'function',
        signature: 'fn urlEncode(text: string): string',
        doc: 'Percent-encodes text according to RFC 3986 unreserved characters.',
        snippet: 'urlEncode(${1:text})'
      },
      urlDecode: {
        name: 'urlDecode',
        kind: 'function',
        signature: 'fn urlDecode(text: string): string',
        doc: 'Decodes RFC 3986 percent-encoded sequences (%XX) and + spaces.',
        snippet: 'urlDecode(${1:text})'
      },
      parseUrl: {
        name: 'parseUrl',
        kind: 'function',
        signature: 'fn parseUrl(url: string): map<string, string>',
        doc: 'Parses url into keys "scheme", "host", "port", "path", "query", and "fragment".',
        snippet: 'parseUrl(${1:url})'
      },
      urlScheme: {
        name: 'urlScheme',
        kind: 'function',
        signature: 'fn urlScheme(url: string): string',
        doc: 'Extracts the lowercase scheme ("http", "https", etc.) from url.',
        snippet: 'urlScheme(${1:url})'
      },
      urlHost: {
        name: 'urlHost',
        kind: 'function',
        signature: 'fn urlHost(url: string): string',
        doc: 'Extracts the host or unbracketed IPv6 address from url.',
        snippet: 'urlHost(${1:url})'
      },
      urlPort: {
        name: 'urlPort',
        kind: 'function',
        signature: 'fn urlPort(url: string): int',
        doc: 'Returns explicit port from url, or default 80 (http), 443 (https), or 0.',
        snippet: 'urlPort(${1:url})'
      },
      urlPath: {
        name: 'urlPath',
        kind: 'function',
        signature: 'fn urlPath(url: string): string',
        doc: 'Extracts the path component from url (defaults to "/").',
        snippet: 'urlPath(${1:url})'
      },
      urlQuery: {
        name: 'urlQuery',
        kind: 'function',
        signature: 'fn urlQuery(url: string): string',
        doc: 'Extracts the query string (without leading ?) from url.',
        snippet: 'urlQuery(${1:url})'
      },
      buildQuery: {
        name: 'buildQuery',
        kind: 'function',
        signature: 'fn buildQuery(params: map<string, string>): string',
        doc: 'Serializes params into a sorted, URL-encoded k1=v1&k2=v2 query string.',
        snippet: 'buildQuery(${1:params})'
      },
      parseQuery: {
        name: 'parseQuery',
        kind: 'function',
        signature: 'fn parseQuery(query: string): map<string, string>',
        doc: 'Parses a query string into a decoded map<string, string>.',
        snippet: 'parseQuery(${1:query})'
      },
      resolveHost: {
        name: 'resolveHost',
        kind: 'function',
        signature: 'fn resolveHost(host: string): array<string>',
        doc: 'Resolves host via DNS into a list of IPv4 and IPv6 addresses.',
        snippet: 'resolveHost("${1:localhost}")'
      },
      resolveIpv4: {
        name: 'resolveIpv4',
        kind: 'function',
        signature: 'fn resolveIpv4(host: string): string',
        doc: 'Resolves host to its first IPv4 address string.',
        snippet: 'resolveIpv4("${1:localhost}")'
      },
      isIpv4: {
        name: 'isIpv4',
        kind: 'function',
        signature: 'fn isIpv4(addr: string): bool',
        doc: 'Returns true if addr is a valid dotted-quad IPv4 address.',
        snippet: 'isIpv4(${1:addr})'
      },
      isIpv6: {
        name: 'isIpv6',
        kind: 'function',
        signature: 'fn isIpv6(addr: string): bool',
        doc: 'Returns true if addr is a valid IPv6 address.',
        snippet: 'isIpv6(${1:addr})'
      },
      isIp: {
        name: 'isIp',
        kind: 'function',
        signature: 'fn isIp(addr: string): bool',
        doc: 'Returns true if addr is either a valid IPv4 or IPv6 address.',
        snippet: 'isIp(${1:addr})'
      },
      isLoopback: {
        name: 'isLoopback',
        kind: 'function',
        signature: 'fn isLoopback(addr: string): bool',
        doc: 'Returns true if addr is "localhost", 127.0.0.0/8, or ::1.',
        snippet: 'isLoopback(${1:addr})'
      },
      hostname: {
        name: 'hostname',
        kind: 'function',
        signature: 'fn hostname(): string',
        doc: 'Returns the local machine network hostname.',
        snippet: 'hostname()'
      },
      get: {
        name: 'get',
        kind: 'function',
        signature: 'fn get(url: string): string',
        doc: 'Performs an HTTP/HTTPS GET request and returns the response body.',
        snippet: 'get("${1:http://example.com}")'
      },
      getStatus: {
        name: 'getStatus',
        kind: 'function',
        signature: 'fn getStatus(url: string): int',
        doc: 'Performs an HTTP/HTTPS GET request and returns the HTTP status code.',
        snippet: 'getStatus("${1:http://example.com}")'
      },
      post: {
        name: 'post',
        kind: 'function',
        signature: 'fn post(url: string, body: string, contentType: string): string',
        doc: 'Performs an HTTP/HTTPS POST request and returns the response body.',
        snippet: 'post(${1:url}, ${2:body}, "${3:text/plain}")'
      },
      postJson: {
        name: 'postJson',
        kind: 'function',
        signature: 'fn postJson(url: string, jsonBody: string): string',
        doc: 'Performs an HTTP/HTTPS POST request with Content-Type: application/json.',
        snippet: 'postJson(${1:url}, ${2:jsonBody})'
      },
      put: {
        name: 'put',
        kind: 'function',
        signature: 'fn put(url: string, body: string, contentType: string): string',
        doc: 'Performs an HTTP/HTTPS PUT request and returns the response body.',
        snippet: 'put(${1:url}, ${2:body}, "${3:application/json}")'
      },
      deleteReq: {
        name: 'deleteReq',
        kind: 'function',
        signature: 'fn deleteReq(url: string): string',
        doc: 'Performs an HTTP/HTTPS DELETE request and returns the response body.',
        snippet: 'deleteReq(${1:url})'
      },
      request: {
        name: 'request',
        kind: 'function',
        signature: 'fn request(method: string, url: string, headers: map<string, string>, body: string, timeoutMs: int): string',
        doc: 'Executes a customizable HTTP/HTTPS request and returns a JSON response envelope.',
        snippet: 'request("${1:GET}", ${2:url}, ${3:headers}, ${4:""}, ${5:5000})'
      },
      download: {
        name: 'download',
        kind: 'function',
        signature: 'fn download(url: string, destPath: string): bool',
        doc: 'Downloads url to destPath and returns true if HTTP status is 200..299.',
        snippet: 'download(${1:url}, ${2:destPath})'
      },
      tcpConnect: {
        name: 'tcpConnect',
        kind: 'function',
        signature: 'fn tcpConnect(host: string, port: int): int',
        doc: 'Opens a TCP connection to host:port and returns a stream socket handle.',
        snippet: 'tcpConnect("${1:127.0.0.1}", ${2:8080})'
      },
      tcpConnectTimeout: {
        name: 'tcpConnectTimeout',
        kind: 'function',
        signature: 'fn tcpConnectTimeout(host: string, port: int, timeoutMs: int): int',
        doc: 'Opens a TCP connection to host:port with an explicit timeout in milliseconds.',
        snippet: 'tcpConnectTimeout("${1:127.0.0.1}", ${2:8080}, ${3:3000})'
      },
      tcpListen: {
        name: 'tcpListen',
        kind: 'function',
        signature: 'fn tcpListen(host: string, port: int, backlog: int): int',
        doc: 'Binds and listens on host:port and returns a TCP listener handle.',
        snippet: 'tcpListen("${1:127.0.0.1}", ${2:0}, ${3:16})'
      },
      tcpAccept: {
        name: 'tcpAccept',
        kind: 'function',
        signature: 'fn tcpAccept(listenerHandle: int): int',
        doc: 'Accepts an incoming connection on listenerHandle and returns a stream socket handle.',
        snippet: 'tcpAccept(${1:listenerHandle})'
      },
      tcpLocalPort: {
        name: 'tcpLocalPort',
        kind: 'function',
        signature: 'fn tcpLocalPort(handle: int): int',
        doc: 'Returns the bound local port number of a TCP listener or stream handle.',
        snippet: 'tcpLocalPort(${1:handle})'
      },
      tcpPeerAddr: {
        name: 'tcpPeerAddr',
        kind: 'function',
        signature: 'fn tcpPeerAddr(handle: int): string',
        doc: 'Returns the remote peer address formatted as "ip:port".',
        snippet: 'tcpPeerAddr(${1:handle})'
      },
      tcpSend: {
        name: 'tcpSend',
        kind: 'function',
        signature: 'fn tcpSend(handle: int, data: string): int',
        doc: 'Sends all bytes of data over the TCP stream and returns total bytes sent.',
        snippet: 'tcpSend(${1:handle}, ${2:data})'
      },
      tcpRecv: {
        name: 'tcpRecv',
        kind: 'function',
        signature: 'fn tcpRecv(handle: int, maxBytes: int): string',
        doc: 'Reads up to maxBytes from the TCP stream.',
        snippet: 'tcpRecv(${1:handle}, ${2:4096})'
      },
      tcpRecvLine: {
        name: 'tcpRecvLine',
        kind: 'function',
        signature: 'fn tcpRecvLine(handle: int, maxBytes: int): string',
        doc: 'Reads a line up to \\n (stripping trailing \\r\\n or \\n) up to maxBytes.',
        snippet: 'tcpRecvLine(${1:handle}, ${2:4096})'
      },
      tcpSetTimeout: {
        name: 'tcpSetTimeout',
        kind: 'function',
        signature: 'fn tcpSetTimeout(handle: int, timeoutMs: int): void',
        doc: 'Sets send and receive timeouts on handle in milliseconds.',
        snippet: 'tcpSetTimeout(${1:handle}, ${2:3000})'
      },
      tcpClose: {
        name: 'tcpClose',
        kind: 'function',
        signature: 'fn tcpClose(handle: int): void',
        doc: 'Closes the TCP socket handle and removes it from the registry.',
        snippet: 'tcpClose(${1:handle})'
      },
      udpBind: {
        name: 'udpBind',
        kind: 'function',
        signature: 'fn udpBind(host: string, port: int): int',
        doc: 'Binds a UDP socket on host:port and returns a UDP handle.',
        snippet: 'udpBind("${1:127.0.0.1}", ${2:0})'
      },
      udpLocalPort: {
        name: 'udpLocalPort',
        kind: 'function',
        signature: 'fn udpLocalPort(handle: int): int',
        doc: 'Returns the bound local port number of the UDP socket handle.',
        snippet: 'udpLocalPort(${1:handle})'
      },
      udpSendTo: {
        name: 'udpSendTo',
        kind: 'function',
        signature: 'fn udpSendTo(handle: int, host: string, port: int, data: string): int',
        doc: 'Sends datagram data to host:port and returns bytes transmitted.',
        snippet: 'udpSendTo(${1:handle}, "${2:127.0.0.1}", ${3:port}, ${4:data})'
      },
      udpRecvFrom: {
        name: 'udpRecvFrom',
        kind: 'function',
        signature: 'fn udpRecvFrom(handle: int, maxBytes: int): string',
        doc: 'Receives a datagram up to maxBytes and returns JSON {"data":"...","host":"...","port":...}.',
        snippet: 'udpRecvFrom(${1:handle}, ${2:1024})'
      },
      udpClose: {
        name: 'udpClose',
        kind: 'function',
        signature: 'fn udpClose(handle: int): void',
        doc: 'Closes the UDP socket handle.',
        snippet: 'udpClose(${1:handle})'
      }
    }
  },
  http: {
    name: 'http',
    doc: 'High-performance HTTP/1.1, WebSocket, and Reactive UI (.kml) LiveView server.',
    members: {
      app: {
        name: 'app',
        kind: 'function',
        signature: 'fn app(reusePort?: bool): Server',
        doc: 'Creates a new Express-style HTTP server application.',
        snippet: 'app()'
      },
      createServer: {
        name: 'createServer',
        kind: 'function',
        signature: 'fn createServer(reusePort?: bool): Server',
        doc: 'Creates a new HTTP server instance.',
        snippet: 'createServer()'
      },
      cors: {
        name: 'cors',
        kind: 'function',
        signature: 'fn cors(): Middleware',
        doc: 'Enables Cross-Origin Resource Sharing (CORS) headers.',
        snippet: 'cors()'
      },
      logger: {
        name: 'logger',
        kind: 'function',
        signature: 'fn logger(): Middleware',
        doc: 'Logs incoming HTTP requests, status codes, and response latency.',
        snippet: 'logger()'
      },
      jsonParser: {
        name: 'jsonParser',
        kind: 'function',
        signature: 'fn jsonParser(): Middleware',
        doc: 'Parses incoming JSON request bodies.',
        snippet: 'jsonParser()'
      },
      staticDir: {
        name: 'staticDir',
        kind: 'function',
        signature: 'fn staticDir(dir: string): Middleware',
        doc: 'Serves static files from disk with ETag and MIME support.',
        snippet: 'staticDir("${1:./public}")'
      },
      recover: {
        name: 'recover',
        kind: 'function',
        signature: 'fn recover(): Middleware',
        doc: 'Recovers from unhandled errors and responds with HTTP 500.',
        snippet: 'recover()'
      }
    }
  },
  crypto: {
    name: 'crypto',
    doc: 'Cryptographic hashes, HMAC, AEAD ciphers, CSPRNG, Base64/Hex, and QR/Code128 symbology.',
    members: {
      sha256: {
        name: 'sha256',
        kind: 'function',
        signature: 'fn sha256(data: string): string',
        doc: 'Computes the lowercase hex SHA-256 digest of data.',
        snippet: 'sha256(${1:data})'
      },
      sha512: {
        name: 'sha512',
        kind: 'function',
        signature: 'fn sha512(data: string): string',
        doc: 'Computes the lowercase hex SHA-512 digest of data.',
        snippet: 'sha512(${1:data})'
      },
      blake3: {
        name: 'blake3',
        kind: 'function',
        signature: 'fn blake3(data: string): string',
        doc: 'Computes the 256-bit BLAKE3 hex digest of data.',
        snippet: 'blake3(${1:data})'
      },
      randomUuid: {
        name: 'randomUuid',
        kind: 'function',
        signature: 'fn randomUuid(): string',
        doc: 'Generates a cryptographically random RFC 4122 UUID v4 string.',
        snippet: 'randomUuid()'
      },
      randomHex: {
        name: 'randomHex',
        kind: 'function',
        signature: 'fn randomHex(byteCount: int): string',
        doc: 'Generates random bytes encoded as lowercase hex.',
        snippet: 'randomHex(${1:16})'
      },
      toBase64: {
        name: 'toBase64',
        kind: 'function',
        signature: 'fn toBase64(data: string): string',
        doc: 'Encodes data into standard RFC 4648 Base64.',
        snippet: 'toBase64(${1:data})'
      },
      fromBase64: {
        name: 'fromBase64',
        kind: 'function',
        signature: 'fn fromBase64(b64: string): string',
        doc: 'Decodes an RFC 4648 Base64 string.',
        snippet: 'fromBase64(${1:b64})'
      }
    }
  },
  test: {
    name: 'test',
    doc: 'Unit testing framework, suites, test cases, and assertion primitives.',
    members: {
      suite: {
        name: 'suite',
        kind: 'function',
        signature: 'fn suite(name: string, body: fn(): void): void',
        doc: 'Defines a test suite grouping related test cases.',
        snippet: 'suite("${1:Suite Name}", fn(): void {\n\t$0\n})'
      },
      case: {
        name: 'case',
        kind: 'function',
        signature: 'fn case(name: string, body: fn(): void): void',
        doc: 'Registers an individual unit test case.',
        snippet: 'case("${1:test description}", fn(): void {\n\t$0\n})'
      },
      assertEq: {
        name: 'assertEq',
        kind: 'function',
        signature: 'fn assertEq(actual: any, expected: any, msg?: string): void',
        doc: 'Asserts that actual and expected values are deeply equal.',
        snippet: 'assertEq(${1:actual}, ${2:expected})'
      },
      assertTrue: {
        name: 'assertTrue',
        kind: 'function',
        signature: 'fn assertTrue(cond: bool, msg?: string): void',
        doc: 'Asserts that condition evaluates to true.',
        snippet: 'assertTrue(${1:cond})'
      }
    }
  },
  ui: {
    name: 'ui',
    doc: 'Compile-time Reactive UI engine, scene graph nodes, event modifiers, and TUI/Web/Headless backends.',
    members: {
      debounce: {
        name: 'debounce',
        kind: 'function',
        signature: 'fn debounce(delayMs: int, handler: fn(UIEvent): void): EventCallback',
        doc: 'Debounces an event listener by delayMs milliseconds.',
        snippet: 'debounce(${1:200}, ${2:handler})'
      },
      withPreventDefault: {
        name: 'withPreventDefault',
        kind: 'function',
        signature: 'fn withPreventDefault(handler: fn(UIEvent): void): EventCallback',
        doc: 'Calls preventDefault() on the event before invoking handler.',
        snippet: 'withPreventDefault(${1:handler})'
      },
      withKeyFilter: {
        name: 'withKeyFilter',
        kind: 'function',
        signature: 'fn withKeyFilter(key: string, handler: fn(UIEvent): void): EventCallback',
        doc: 'Invokes handler only when the pressed key matches key.',
        snippet: 'withKeyFilter("${1:Enter}", ${2:handler})'
      }
    }
  }
};

export const BUILTIN_METHODS: Record<string, StdlibMember> = {
  length: {
    name: 'length',
    kind: 'function',
    signature: 'fn length(): int',
    doc: 'Returns the number of elements or characters.',
    snippet: 'length()'
  },
  toString: {
    name: 'toString',
    kind: 'function',
    signature: 'fn toString(): string',
    doc: 'Converts value into its string representation.',
    snippet: 'toString()'
  },
  tryInt: {
    name: 'tryInt',
    kind: 'function',
    signature: 'fn tryInt(): int | null',
    doc: 'Attempts to parse a string into an integer, returning null if invalid.',
    snippet: 'tryInt()'
  },
  toInt: {
    name: 'toInt',
    kind: 'function',
    signature: 'fn toInt(): int',
    doc: 'Converts a number or parses a string to int (raises error on failure).',
    snippet: 'toInt()'
  },
  toFloat: {
    name: 'toFloat',
    kind: 'function',
    signature: 'fn toFloat(): float',
    doc: 'Converts a number or parses a string to float.',
    snippet: 'toFloat()'
  },
  trim: {
    name: 'trim',
    kind: 'function',
    signature: 'fn trim(): string',
    doc: 'Returns a copy of the string with leading and trailing whitespace removed.',
    snippet: 'trim()'
  },
  startsWith: {
    name: 'startsWith',
    kind: 'function',
    signature: 'fn startsWith(prefix: string): bool',
    doc: 'Returns true if the string begins with prefix.',
    snippet: 'startsWith(${1:prefix})'
  },
  endsWith: {
    name: 'endsWith',
    kind: 'function',
    signature: 'fn endsWith(suffix: string): bool',
    doc: 'Returns true if the string ends with suffix.',
    snippet: 'endsWith(${1:suffix})'
  },
  split: {
    name: 'split',
    kind: 'function',
    signature: 'fn split(delimiter: string): array<string>',
    doc: 'Splits the string into substrings separated by delimiter.',
    snippet: 'split(${1:delimiter})'
  },
  contains: {
    name: 'contains',
    kind: 'function',
    signature: 'fn contains(element: any): bool',
    doc: 'Returns true if string contains substring or array contains item.',
    snippet: 'contains(${1:element})'
  },
  push: {
    name: 'push',
    kind: 'function',
    signature: 'fn push(element: any): void',
    doc: 'Appends an element to the end of the array.',
    snippet: 'push(${1:element})'
  },
  pop: {
    name: 'pop',
    kind: 'function',
    signature: 'fn pop(): any',
    doc: 'Removes and returns the last element of the array.',
    snippet: 'pop()'
  },
  keys: {
    name: 'keys',
    kind: 'function',
    signature: 'fn keys(): array<any>',
    doc: 'Returns an array of all keys in the map.',
    snippet: 'keys()'
  },
  values: {
    name: 'values',
    kind: 'function',
    signature: 'fn values(): array<any>',
    doc: 'Returns an array of all values in the map.',
    snippet: 'values()'
  }
};

export const BUILTIN_TYPES = [
  { name: 'int', doc: '64-bit signed integer' },
  { name: 'float', doc: '64-bit IEEE 754 floating-point number' },
  { name: 'bool', doc: 'Boolean value (true or false)' },
  { name: 'char', doc: 'Unicode code point character' },
  { name: 'string', doc: 'Immutable UTF-8 string' },
  { name: 'void', doc: 'Absence of a return value' },
  { name: 'null', doc: 'Null reference or empty optional' },
  { name: 'error', doc: 'Standard error object with message and code' },
  { name: 'array', doc: 'Dynamically sized ordered sequence array<T>' },
  { name: 'map', doc: 'Key-value associative hash map<K, V>' }
];
