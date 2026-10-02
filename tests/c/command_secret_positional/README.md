# command_secret_positional test

`gclass_create()` refuses a command table where a required secret parameter
(`SDF_SECRET`, or a secret's name, as the command parser tells them) is
followed by another required parameter, with one ERROR (*"A required secret
parameter must be the last required one"*). A required parameter can be given
without its key and the line is split by blanks: a secret written with blanks
would give a piece of itself to the next parameter (shown in the command
traces and in its errors) and echo the rest in the "extra parameters" answer.
As the last required parameter, what spills is extra text, which the parser
takes as the rest of the secret and never shows.

1. a required secret followed by a required parameter: refused, one ERROR;
2. the same with a secret by its name (`password`, no flag): refused;
3. a required secret as the only required parameter, optional ones after it,
4. a required parameter then a required secret,
5. a secret that is not required (`key=value` only) before others:
   accepted, no ERROR.

Up to 7.25.21 such a table was accepted.

## Run

```bash
ctest -R command_secret_positional --output-on-failure --test-dir build
```
