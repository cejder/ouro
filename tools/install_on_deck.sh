#!/usr/bin/env bash
make build BUILD_TYPE=Release build

# Create the target directory structure directly during rsync
sshpass -p "sdf" rsync -avz --progress \
        build/ouro \
        build/libmimalloc.so.2 \
        build/libmimalloc.so.2.2 \
        assets \
        tapes \
        third_party/fmod/api/core/lib_linux/x86_64/libfmodL.so.14 \
        third_party/fmod/api/core/lib_linux/x86_64/libfmodL.so.14.9 \
        tools/run_on_deck.sh \
        "deck@192.168.0.244:/home/deck/.local/bin/ouro/"

# Rename run_on_deck.sh to run.sh on the remote
sshpass -p "sdf" ssh deck@192.168.0.244 "mv /home/deck/.local/bin/ouro/run_on_deck.sh /home/deck/.local/bin/ouro/run.sh"
