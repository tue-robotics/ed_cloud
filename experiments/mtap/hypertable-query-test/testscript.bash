#!/bin/bash

export NODENAME=$1
export DB_ADDRESS=db${1}.internal.wambacloud.com


export DIE_OPERATIONS=$(($NUM_OPERATIONS + 5000000))

cat << EOF | tee profile-query-temp.yaml
externals:
- name: $NODENAME
plugins:
- name: hypertable_writer_plugin
  lib: libed_hypertable_writer_plugin.so
  parameters:
      address: $DB_ADDRESS
      port: 15867
      stop: $NUM_OPERATIONS
      namespace: test
      stop: -1
      profile: 1
      drop_table: 1
      write:
        - property: shape
        - property: pose
        - property: convex_hull
        - property: type
  frequency: $FREQUENCY
- name: hypertable_reader_plugin
  lib: libed_hypertable_reader_plugin.so
  parameters:
      address: $DB_ADDRESS
      port: 15867
      namespace: test
      stop: $NUM_OPERATIONS
      profile: 1
      stop: -1
      read:
        - property: shape
        - property: pose
        - property: convex_hull
        - property: type
  frequency: $FREQUENCY
- name: bouncing_cubes
  lib: libed_bouncing_cubes.so
  parameters:
       num_cubes: $NUM_ELEMENTS
       ns: r
       stop: $NUM_OPERATIONS
       die: $DIE_OPERATIONS
       measurement_width: 4
       measurement_height: 4
  frequency: $FREQUENCY
EOF

echo "Executing for NUM_OPERATIONS=$NUM_OPERATIONS NUM_ELEMENTS=$NUM_ELEMENTS FREQUENCY=$FREQUENCY"
LD_LIBRARY_PATH=/opt/hypertable/current/lib/:$LD_LIBRARY_PATH timeout 120 /home/ubuntu/ros/indigo/system/devel/lib/ed/ed profile-query-temp.yaml  _name:=$1 | tee test-${NODENAME}-${NUM_OPERATIONS}o-${NUM_ELEMENTS}e-${FREQUENCY}hz-a$i.txt
scp *.txt centos@lemarq.wambacluster.com:/home/centos/res/${NODES}node/
rm *.txt

rm profile-query-temp.yaml
