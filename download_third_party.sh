mkdir third_party_sources ||:
cd third_party_sources

wget https://sourceforge.net/projects/boost/files/boost/1.67.0/boost_1_67_0.tar.gz
tar zxvf boost_1_67_0.tar.gz
mv boost_1_67_0 boost
rm boost_1_67_0.tar.gz
