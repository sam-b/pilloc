# pilloc
A pin tool to visualise heap operations.
![image](/pilloc_example.png)
# Build and Run
To Build run:
```shell
make PIN_ROOT=~/code/pin-2.14-71313-gcc.4.4.7-linux
```
Where PIN_ROOT is the path to your pin directory.
Then to run:
```shell
sudo ~/code/pin-2.14-71313-gcc.4.4.7-linux/pin -t obj-intel64/pilloc.so -- /bin/cat errors.txt
```
Where "~/code/pin-2.14-71313-gcc.4.4.7-linux/pin" is replaced with your path to pin
# Notes
Idea and html stolen from @wapiflapi
This is super buggy and incomplete atm