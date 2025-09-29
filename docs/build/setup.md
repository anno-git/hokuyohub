'/etc/NetworkManager/system-connections/Wired connection 1.nmconnection'


sudo nmcli con mod "Wired connection 1" \
ipv4.method "manual" \
ipv4.addresses "192.168.104.10/24" \
ipv4.gateway "192.168.104.1" 

sudo nmcli connection reload
sudo nmcli connection up "Wired connection 1"


sudo apt-get install libyaml-cpp0.7 libjsoncpp25 libnng1

sudo systemctl enable systemd-networkd-wait-online.service

[Unit]
Description=HokuyoHub
After=network.target
Wants=network.target

[Service]
ExecStart=/home/annolab/apps/hokuyo_hub/hokuyo_hub
WorkingDirectory=/home/annolab/apps/hokuyo_hub
User=annolab
Group=annolab
Restart=always
RestartSec=0
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target