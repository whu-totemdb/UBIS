# UBIS

## Build
Ensure the following dependencies are downloaded:
```
sudo apt install cmake
sudo apt install libjemalloc-dev libsnappy-dev libgflags-dev
sudo apt install pkg-config
sudo apt install swig libboost-all-dev
sudo apt install libtbb-dev
sudo apt install libisal-dev
```

Clone the repository:
```
git clone https://github.com/whu-totemdb/UBIS.git
cd UBIS
git submodule update --init --recursive
```

Install spdk in your environment:
```
cd ThirdParty/spdk
sudo ./scripts/pkgdep.sh
./configure
make -j
```

Install isa-l_crypto in your environment:
```
cd ThirdParty/isal-l_crypto
sudo ./autogen.sh
./configure
make -j
```

Install RocksDB engine:
```
git clone https://github.com/PtilopsisL/rocksdb.git
cd rocksdb
mkdir build && cd build
cmake -DUSE_RTTI=1 -DWITH_JEMALLOC=1 -DWITH_SNAPPY=1 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-fPIC" ..
make -j
sudo make install
```

Finally, build UBIS:
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j
```






