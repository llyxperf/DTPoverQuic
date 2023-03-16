# DTP over QUIC

## Build

Build by calling: 
`cmake -S. -Bbuild && cmake --build build`

## Test

To enable debug_log, you can add `-v 4` after all other options.

### QUIC Version

Test the basic transport function matching block to quic stream.

Use `-q` parameter to enable this test.

```sh
$ cmake -S. -Bbuild && cmake --build build
$ cp aitrans_block.txt examples/cert.crt examples/cert.key examples/rootca.crt ./build
$ cd build
$ ./bin/server 127.0.0.1 5555 aitrans_block.txt -q -l server.log # start the server
$ ./bin/client 127.0.0.1 5555 127.0.0.1 6666 -q -o output.csv -l client.log # start the client
```

### Dgram version

Run the example programs without `-q` to enable dgram transmittion.

```sh
$ cmake -S. -Bbuild && cmake --build build
$ cp aitrans_block.txt examples/cert.crt examples/cert.key examples/rootca.crt ./build
$ cd build
$ ./bin/server 127.0.0.1 5555 aitrans_block.txt -l server.log # start the server
$ ./bin/client 127.0.0.1 5555 127.0.0.1 6666 -o output.csv -l client.log # start the client
```

```sh
# start traffic control
# set the bandwidth of nic loopback 1000mbps, add 5ms delay 0.01% loss and 10% reordering
$ sudo python traffic_control.py -once -dl 0.005 -nic lo -bw 1000 -loss 0.01 -re 10%
# reset
$ sudo python traffic_control.py -r lo
```

## Some Q&A

### CPM command doesn't exist

You may get this error if CPM is not downloaded successfully. Please go to github.com and download `cmake/CPM.cmake` into `build/cmake/CPM_{VERSION}.cmake`
