# Mini Shell in C

A simple UNIX shell written in C using POSIX system calls.

## Features

- Run external commands with `fork()`, `execv()`, and `waitpid()`
- Built-in `cd` and `exit` commands
- One pipe between two commands using `pipe()` and `dup2()`
- Custom command tokenizer with dynamic allocation
- Executable lookup in `/bin`, `/usr/bin`, and `/usr/local/bin`

## Build and Run

```sh
make
./mini-shell
```

## Example Commands

```sh
pwd
ls -l
cd ..
cat file.txt | wc -l
exit
```

## Design

The shell reads a line with `getline()`, splits it into tokens, and checks for a pipe or built-in command. External commands are launched in child processes. For a pipe, two children run concurrently, with standard output and input connected through file descriptors. The parent waits for the children and frees the input memory.

## Limitations

- Only one pipe; `|` must have spaces around it
- No quotes, redirection, or background execution
- Fixed executable search directories (does not use `$PATH`)
- Maximum 64 tokens per command

## Clean

```sh
make clean
```
