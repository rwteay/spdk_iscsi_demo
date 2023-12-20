#!/usr/bin/env bash

./scripts/rpc.py bdev_usr_scsi_create 1000000 512 -b Malloc0 -dirver 2 -disk 0
./scripts/rpc.py bdev_usr_scsi_create 1000000 512 -b Malloc1 -dirver 2 -disk 1
# ./scripts/rpc.py bdev_malloc_create -b Malloc0 64 512
# ./scripts/rpc.py bdev_malloc_create -b Malloc1 64 512

./scripts/rpc.py iscsi_create_portal_group 1 10.0.8.29:3260 # 后端服务器的IP
./scripts/rpc.py iscsi_create_initiator_group 2 ANY 10.0.8.21/32 # 前端服务器的IP
./scripts/rpc.py iscsi_create_target_node disk1 "Data Disk1" "Malloc0:0 Malloc1:1" 1:2 64 -d

# ./build/bin/iscsi_tgt