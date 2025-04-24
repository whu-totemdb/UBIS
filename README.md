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
**Note:** The script "pkgdep.sh" is used to check the required dependencies in your environment. You can also use other scripts in the folder `/scripts/pkgdep/` to examine your local environment. For example, if you are running in the Ubuntu environment, you can use command `sudo ./scripts/pkgdep/debian.sh`.

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
**Note:**
+ If some dependencies are missing, just use `apt` command to install them. For example, if the log shows that "could not find uring", then use `sudo apt -y install liburing-dev` to download the required dependency.
+ To compile rocksdb successfully, the version of g++ and gcc compilers is best to be 9 or 10. You can pass the target gcc/g++ version in the `cmake` command, such as `cmake -DUSE_RTTI=1 -DWITH_JEMALLOC=1 -DWITH_SNAPPY=1 -DCMAKE_C_COMPILER=gcc-10 -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-fPIC" .`

Finally, build UBIS:
```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j
```
**Note:** 
+ The gcc/g++ compiler should be consistent with that used for compiling rocksdb.
+ 2 executable files (**usefultool** & **ubis**) are used. Please refer to [the json file](.vscode/launch.json) to learn about how to run the programs. You should change the paths to the data files in your environment.

## Datasets
+ [Argoverse 2 motion forcasting dataset](https://www.argoverse.org/av2.html): please refer to [the codes](/data/preprocess/argoverse2) to preprocess the raw data.
+ [SIFT1M dataset](http://corpus-texmex.irisa.fr/): please refer to [the codes](/data/preprocess/sift1m) to preprocess the raw data.

After preprocessing, three binary files are generated: 
+ base_embeddings.bin: it contains all the base vectors and they are sorted in chronological order.
+ query_embeddings.bin: it contains all the query vectors and they are sorted in chronological order.
+ query_vector_range.bin: it is a dense array, and it indicates the order between a query vector and its corresponding base vectors. For example, `query_vector_range[i] = j` means that `j` base vectors are visible for the `i-th` query vector. It is equivalent to the timestamp of the `i-th` query vector is greater than any timestamps of these `j` base vectors.

As the dataset for updating index shifts dynamically, the ground truths are also evolving. Function `genereteTruthSplittedByRide` is used to generate the evolving ground truth (but this process usually consumes much time).


## Comparisons
We modify some codes of the existing methods to make them support the input formats above. 
#### 1 FreshDiskANN
##### Build
clone the repository, install the dependencies and build:
```
git clone https://github.com/Ryanhya/Comparison_FreshDiskANN.git
cd Comparison_FreshDiskANN
sudo apt install libgoogle-perftools-dev clang-format
wget https://registrationcenter-download.intel.com/akdlm/irc_nas/18487/l_BaseKit_p_2022.1.2.146_offline.sh
sudo sh ./l_BaseKit_p_2022.1.2.146_offline.sh
```


build FreshDiskANN:
```
mkdir build
cd build
cmake ..
make -j
```

#### 2 SPFresh
##### Build
clone the repository:
```
git clone https://github.com/Ryanhya/Comparison_SPFresh.git
cd Comparison_SPFresh
git submodule update --init --recursive
```

The installation of other dependencies is the same as UBIS. Please follow the same steps for installing spdk, isa-l_crypto and rocksdb.



