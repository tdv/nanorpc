WORKING_DIR=$PWD
BOOST_DIR=$WORKING_DIR/third_party/boost/
BOOST_VER=1.69.0
BOOST_VER_=$(echo $BOOST_VER | tr . _)
echo $BOOST_VER $BOOST_VER_


mkdir third_party
cd third_party

wget https://boostorg.jfrog.io/artifactory/main/release/$BOOST_VER/source/boost_$BOOST_VER_.tar.gz
tar zxvf boost_$BOOST_VER_.tar.gz
mv boost_$BOOST_VER_ boost_sources
cd boost_sources

./bootstrap.sh --prefix=$BOOST_DIR \
    --with-libraries=iostreams,date_time,thread,system \
    --without-icu
./b2 install -j8 --disable-icu --ignore-site-config "cxxflags=-std=c++17 -fPIC" \
    link=static threading=multi runtime-link=static

cd $WORKING_DIR
mkdir build
cd build

cmake -DBOOST_ROOT=$BOOST_DIR \
    -DCMAKE_INSTALL_PREFIX=$WORKING_DIR/target/nanorpc ..
make install
