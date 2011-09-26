HOST=`hostname -f`

cd ~/$HOST/src/mvctp_v2
make
make clean
sudo cp ./emustarter ~/$HOST/
sudo cp ./config ~/$HOST/
sudo chmod +x ./rate-limit.sh
sudo cp ./rate-limit.sh ~/$HOST/
sudo chmod +x ./loss-rate-tcp.sh
sudo cp ./loss-rate-tcp.sh ~/$HOST/
cd ~/$HOST
sudo ./emustarter config

