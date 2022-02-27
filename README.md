# my_tar
My version of tar (USTar (POSIX.1-1988)) library made with C, because C is mother of all languages so everybody should try it. 


Real _tar_ supports a lot of switches. I limited myself in this task
to just a few of them:

One-character switches are entered together without a space and without the `-` character
(eg `tar xv archive.tar`) and will always be specified before file paths to package, or archive for unpackage.
The `c` and` x` switches cannot be combined.

`c` ::
Command to create a new archive.

`x` ::
Archive extraction command.

`v` ::
During the program run, the name will be written to the standard ** error ** output
the item being processed, each on a separate line.
