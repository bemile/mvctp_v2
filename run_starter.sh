HOST=`hostname -f`

cd ~/$HOST/src/EmulabStarter
make
make clean
sudo cp ./emustarter ~/$HOST/
sudo cp ./config ~/$HOST/
cd ~/$HOST
sudo ./emustarter config

