# Examples

The folder provides series of examples how to use the most interesting parts of Serverbox.

Each example can be built and run on its own, they only depend on Serverbox itself, and not on each other.

They can be used as reference points when you want to try and build something of your own.

The recommended way of reading them is firstly all `scheduler_*`, then all `iocore_*`. The sequence 01, 02, etc is not required, each example is self-sufficient build- and code-wise. But later examples might not explain some simple things already covered in the previous examples.

## Running

The recommended way to use them is this (on non-Windows):
```Bash
# Build them all.
mkdir -p build
cd build
cmake ..
make -j

# Run any of them like this:
./scheduler_01_simple_task/scheduler_01_simple_task
```

On Windows platform they also compile and run, but one would have to run them via cmd/PowerShell/VisualStudio.
