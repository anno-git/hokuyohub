#!/bin/bash
cd "$(dirname "$0")/../dist"
exec ./hokuyo_hub "$@"