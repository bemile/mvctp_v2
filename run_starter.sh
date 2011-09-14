HOST=`hostname -f`

cd ~/$HOST/src/mvctp_v2
make
make clean
sudo cp ./emustarter ~/$HOST/
sudo cp ./config ~/$HOST/
cd ~/$HOST
sudo ./emustarter config

