WORKING_DIR=$PWD

mkdir third_party
cd third_party

wget https://sourceforge.net/projects/boost/files/boost/1.67.0/boost_1_67_0.tar.gz -O boost_1_67_0.tar.gz
tar zxvf boost_1_67_0.tar.gz
mv boost_1_67_0 boost_sources
cd boost_sources

./bootstrap.sh --prefix=$WORKING_DIR/third_party/boost/ \
    --with-libraries=iostreams,date_time,thread,system \
    --without-icu
./b2 install -j8 --disable-icu --ignore-site-config "cxxflags=-std=c++17 -fPIC" \
    link=static threading=multi runtime-link=static

cd $WORKING_DIR
mkdir build
cd build

cmake -DBOOST_ROOT=$WORKING_DIR/third_party/boost/ \
    -DCMAKE_INSTALL_PREFIX=$WORKING_DIR/target/nanorpc ..
make install
