#!/bin/bash

mkdir -p ~/.config/systemd/user
mv ~/parser.service ~/.config/systemd/user

systemctl --user daemon-reload
systemctl --user enable parser.service
systemctl --user start parser.service

loginctl enable-linger $USER
