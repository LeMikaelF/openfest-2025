To open sqlelf in interactive mode:

```sh
docker run -it --rm sqlelf /usr/local/bin/hello-debug
```

To show the 3 most used assembly instructions:

```sh
docker run --rm sqlelf --sql "select mnemonic, COUNT(*) from elf_instructions GROUP BY mnemonic ORDER BY 2 DESC LIMIT 3;" /usr/local/bin/hello-debug 2>/dev/null
```
