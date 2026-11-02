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
        signature: 'fn print(...args: any): void',
        doc: 'Prints arguments to standard output without a trailing newline.',
        snippet: 'print(${1:args})'
      },
      println: {
        name: 'println',
        kind: 'function',
        signature: 'fn println(...args: any): void',
        doc: 'Prints arguments to standard output followed by a newline.',
        snippet: 'println(${1:args})'
      },
      eprint: {
        name: 'eprint',
        kind: 'function',
        signature: 'fn eprint(...args: any): void',
        doc: 'Prints arguments to standard error without a trailing newline.',
        snippet: 'eprint(${1:args})'
      },
      eprintln: {
        name: 'eprintln',
        kind: 'function',
        signature: 'fn eprintln(...args: any): void',
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
        signature: 'fn readLine(): string',
        doc: 'Reads a line of text from standard input (without newline).',
        snippet: 'readLine()'
      },
      input: {
        name: 'input',
        kind: 'function',
        signature: 'fn input(prompt: string): string',
        doc: 'Displays a prompt and reads a line of user input.',
        snippet: 'input("${1:prompt: }")'
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
