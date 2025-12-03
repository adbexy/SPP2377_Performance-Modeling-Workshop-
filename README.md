# SPP-2377_MiniWS_Tutorial_Performance-Models

## Setup

Clone the repository:

```
https://github.com/adbexy/SPP2377_Performance-Modeling-Workshop-.git
```

Now go into the created repository directory (`cd SPP2377_Performance-Modeling-Workshop-`). For further setup (and compilation) we need a very specific environment, so we continue working within a docker container.

```
./start_docker.sh --build
```

Be sure to use the `--build` option when calling the script for the first time. If you don't change the Dockerfile, you can omit the `--build` option in the future.

To finalize the development environment, we now fetch the submodules. (from inside the docker container)

```
./setup.sh
```


## Compile and Test

To check if the installation was successful, we can now build an example program.

```
cmake .

cmake --build . --target simdops_filter

bin/simdops_filter
```

If the commands terminate without any problems, we are ready to go!

## Write Your Own Code

To set up your own program, modify `code/query.cpp`!

Then, use `./run.sh TESTING=[ON|OFF] BUILD_TYPE=[Debug|Release]`.
Your results will be used by `./competition.py` because they are in `my_result`
in this format:

```
<your result>
<correct result>
<throughput>
<throughput>
```

Take care that the `my_result` file is found with this glob:

```bash
ls /home/*/my_results
```

Enjoy coding!
