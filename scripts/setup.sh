export CC=gcc-10
export CXX=g++-10

mkdir tmp
cd tmp
wget https://github.com/ninja-build/ninja/releases/download/v1.10.2/ninja-linux.zip
unzip ninja-linux.zip
sudo mv ninja /usr/local/bin
cd ..
rm -rf tmp
hash -r
