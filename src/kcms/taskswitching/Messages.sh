#! /usr/bin/env bash

# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: BSD-2-Clause

$XGETTEXT `find . -name \*.cpp -o -name \*.h -o -name \*.qml` -o $podir/kcm_kwin_taskswitching.pot
4
