#!/bin/bash

set -u # non initialized variable is an error

# 2 signals are used: SIGUSR1 to decrease delay after level up and SIGUSR2 to quit
# they are sent to all instances of this script
# because of that we should process them in each instance
# in this instance we are ignoring both signals
trap '' SIGUSR1 SIGUSR2

# Those are commands sent to controller by key press processing code
# In controller they are used as index to retrieve actual functuon from array
QUIT=0
RIGHT=1
LEFT=2
ROTATE=3
DOWN=4
DROP=5
TOGGLE_HELP=6
TOGGLE_NEXT=7

# initial delay between piece movements
DELAY=1

# Location and size of playfield
PLAYFIELD_W=10
PLAYFIELD_H=20
PLAYFIELD_X=30
PLAYFIELD_Y=1

# Location of score information
SCORE_X=1
SCORE_Y=2

# Location of help information
HELP_X=58
HELP_Y=1

# Next piece location
NEXT_X=14
NEXT_Y=11

GAMEOVER_X=1
GAMEOVER_Y=$((PLAYFIELD_H + 3))

LEVEL_UP=20

no_color=true    # do we use color or not
showtime=true    # controller runs while this flag is true
empty_cell=" ."  # how we draw empty cell
filled_cell="[]" # how we draw filled cell

score=0           # score variable initialization
level=1           # level variable initialization
lines_completed=0 # completed lines counter initialization

# screen_buffer is variable, that accumulates all screen changes
# this variable is printed in controller once per game cycle
puts() {
    screen_buffer+=${1}
}

# move cursor to (x,y) and print string
# (1,1) is upper left corner of the screen
xyprint() {
    puts "\033[${2};${1}H${3}"
}

show_cursor() {
    echo -ne "\033[?25h"
}

hide_cursor() {
    echo -ne "\033[?25l"
}

# foreground color
set_fg() {
    $no_color && return
    puts "\033[3${1}m"
}

# background color
set_bg() {
    $no_color && return
    puts "\033[4${1}m"
}

reset_colors() {
    puts "\033[0m"
}

set_bold() {
    puts "\033[1m"
}

# playfield is 1-dimensional array, data is stored as follows:
# [ a11, a12, ... a1Y, a21, a22, ... a2Y, ... aX1, aX2, ... aXY]
#   |<  1st line   >|  |<  2nd line   >|  ... |<  last line  >|
# X is PLAYFIELD_W, Y is PLAYFIELD_H
# each array element contains cell color value or -1 if cell is empty
redraw_playfield() {
    local j i x y xp yp

    ((xp = PLAYFIELD_X))
    for ((y = 0; y < PLAYFIELD_H; y++)) {
        ((yp = y + PLAYFIELD_Y))
        ((i = y * PLAYFIELD_W))
        xyprint $xp $yp ""
        for ((x = 0; x < PLAYFIELD_W; x++)) {
            ((j = i + x))
            if ((${play_field[$j]} == -1)) ; then
                puts "$empty_cell"
            else
                set_bold
#                set_fg ${iMap[$j]}
#                set_bg ${iMap[$j]}
                puts "$filled_cell"
                reset_colors
            fi
        }
    }
}

update_score() {
    ((lines_completed += $1))
    ((score += ($1 * $1)))
    if (( score > LEVEL_UP * level)) ; then
        ((level++))
        pkill --full --signal SIGUSR1 "/bin/bash $0"
    fi
    xyprint $SCORE_X $SCORE_Y         "Lines completed: $lines_completed"
    xyprint $SCORE_X $((SCORE_Y + 1)) "Level:           $level"
    xyprint $SCORE_X $((SCORE_Y + 2)) "Score:           $score"
}

help=(
"  Use cursor keys"
"       or"
"      s: up"
"a: left,  d: right"
"    space: drop"
"n: toggle show next"
"h: toggle this help"
)

help_on=1 # if this flag is 1 help is visible

toggle_help() {
    local i s

    for ((i = 0; i < ${#help[@]}; i++ )) {
        # ternary assignment: if help_on is 1 use string as is, otherwise substitute all characters with spaces
        ((help_on == 1)) && s="${help[i]}" || s="${help[i]//?/ }"
        xyprint $HELP_X $((HELP_Y + i)) "$s"
    }
    ((help_on = -help_on))
}

piece=(
"00011011"
"0212223210111213"
"0001111201101120"
"0102101100101121"
"01021121101112220111202100101112"
"01112122101112200001112102101112"
"01111221101112210110112101101112"
)

draw_piece() {
    # Arguments:
    # 1 - x, 2 - y, 3 - type, 4 - rotation, 5 - cell content
    local i x y

    for ((i = 0; i < 8; i += 2)) {
        ((x = $1 + ${piece[$3]:$((i + $4 * 8 + 1)):1} * 2))
        ((y = $2 + ${piece[$3]:$((i + $4 * 8)):1}))
        xyprint $x $y "$5"
    }
    reset_colors
}

next_piece=0
next_piece_rotation=0

next_on=-1

draw_next() {
    # Arguments: 1 - string to draw single cell 
    draw_piece $NEXT_X $NEXT_Y $next_piece $next_piece_rotation "$1"
}

clear_next() {
    draw_next "${filled_cell//?/ }"
}

show_next() {
    draw_next "${filled_cell}"
}

toggle_next() {
    ((next_on = -next_on))
    ((next_on == 1)) && show_next || clear_next
}

draw_current() {
    draw_piece $((current_piece_x * 2 + PLAYFIELD_X)) $((current_piece_y + PLAYFIELD_Y)) $current_piece $current_piece_rotation "$1"
}

show_current() {
    draw_current "${filled_cell}"
}

clear_current() {
    draw_current "${empty_cell}"
}

new_piece_location_ok() {
# arguments: 1 - new x coordinate of the piece, 2 - new y coordinate of the piece
# test if piece can be moved to new location
    local j i x y x_test=$1 y_test=$2

    for ((j = 0, i = 1; j < 8; j += 2, i = j + 1)) {
        ((y = ${piece[$current_piece]:$((j + current_piece_rotation * 8)):1} + y_test)) # new y coordinate of piece part
        ((x = ${piece[$current_piece]:$((i + current_piece_rotation * 8)):1} + x_test)) # new x coordinate of piece part
        ((y < 0 || y >= PLAYFIELD_H || x < 0 || x >= PLAYFIELD_W )) && return 1         # check if we are out of the play field
        ((${play_field[y * PLAYFIELD_W + x]} != -1 )) && return 1                       # check if location is already ocupied
    }
    return 0
}

get_random_next() {
    current_piece=$next_piece
    current_piece_rotation=$next_piece_rotation
    ((current_piece_x = (PLAYFIELD_W - 4) / 2))
    ((current_piece_y = 0))
    show_current
    new_piece_location_ok $current_piece_x $current_piece_y || cmd_quit

    ((next_on == 1)) && clear_next
    ((next_piece = RANDOM % ${#piece[@]}))
    ((next_piece_rotation = RANDOM % (${#piece[$next_piece]} / 8)))
    ((next_on == 1)) && show_next
}

draw_border() {
    local i x1 x2 y

    ((x1 = PLAYFIELD_X - 2))
    ((x2 = PLAYFIELD_X + PLAYFIELD_W * 2))
    for ((i = 0; i < PLAYFIELD_H + 1; i++)) {
        ((y = i + PLAYFIELD_Y))
        xyprint $x1 $y "<|"
        xyprint $x2 $y "|>"
    }

    ((y = PLAYFIELD_Y + PLAYFIELD_H))
    for ((i = 0; i < PLAYFIELD_W; i++)) {
        ((x1 = i * 2 + PLAYFIELD_X))
        xyprint $x1 $y '=='
        xyprint $x1 $((y + 1)) "\/"
    }
    reset_colors
}

init() {
    local i x1 x2 y

    # playfield is initialized with -1s (empty cells)
    for ((i = 0; i < PLAYFIELD_H * PLAYFIELD_W; i++)) {
        play_field[$i]=-1
    }

    clear
    hide_cursor
    update_score 0
    toggle_help

    set_bold
#    set_fg $cBorder
#    set_bg $cBorder

    draw_border
    get_random_next
    redraw_playfield
    get_random_next
    toggle_next
}

# this function runs in separate process
# it sends DOWN commands to controller with appropriate delay
ticker() {
    # on SIGUSR2 this process should exit
    trap exit SIGUSR2
    # on SIGUSR1 delay should be decreased, this happens during level ups
    trap 'DELAY=$(awk "BEGIN {print $DELAY * 0.8}")' SIGUSR1

    while true ; do echo -n $DOWN; sleep $DELAY; done
}

# this function processes keyboard input
reader() {
    trap exit SIGUSR2 # this process exits on SIGUSR2
    trap '' SIGUSR1   # SIGUSR1 is ignored
    local -u key a='' b='' cmd esc_ch=$'\x1b'
    # commands is associative array, which maps pressed keys to commands, sent to controller
    declare -A commands=([A]=$ROTATE [C]=$RIGHT [D]=$LEFT
        [_S]=$ROTATE [_A]=$LEFT [_D]=$RIGHT
        [_]=$DROP [_Q]=$QUIT [_H]=$TOGGLE_HELP [_N]=$TOGGLE_NEXT)

    while read -s -n 1 key ; do
        case "$a$b$key" in
            "${esc_ch}["[ACD]) cmd=${commands[$key]} ;; # cursor key
            *${esc_ch}${esc_ch}) cmd=$QUIT ;;           # exit on 2 escapes
            *) cmd=${commands[_$key]:-} ;;              # regular key. If space was pressed $key is empty
        esac
        a=$b   # preserve previous keys
        b=$key
        [ -n "$cmd" ] && echo -n "$cmd"
    done
}

flatten_map() {
    local i j k x y
    for ((i = 0, j = 1; i < 8; i += 2, j += 2)) {
        ((y = ${piece[$current_piece]:$((i + current_piece_rotation * 8)):1} + current_piece_y))
        ((x = ${piece[$current_piece]:$((j + current_piece_rotation * 8)):1} + current_piece_x))
        ((k = y * PLAYFIELD_W + x))
        play_field[$k]=1 # TODO this should be changed after implementing color support
    }
}

process_complete_lines() {
    local j i complete_lines
    ((complete_lines = 0))
    for ((j = 0; j < PLAYFIELD_W * PLAYFIELD_H; j += PLAYFIELD_W)) ; do
        for ((i = j + PLAYFIELD_W - 1; i >= j; i--)) ; do
            ((${play_field[$i]} == -1)) && break
        done
        ((i >= j)) && continue
        ((complete_lines++))
        for ((i = j - 1; i >= 0; i--)) ; do
            play_field[$((i + PLAYFIELD_W))]=${play_field[$i]}
        done
        for ((i = 0; i < PLAYFIELD_W; i++)) ; do
            play_field[$i]=-1
        done
    done
    return $complete_lines
}

process_fallen_piece() {
    flatten_map
    process_complete_lines && return
    update_score $?
    redraw_playfield
}

move_piece() {
# arguments: 1 - new x coordinate, 2 - new y coordinate
# moves the piece to the new location if possible
    if new_piece_location_ok $1 $2 ; then # if new location is ok
        clear_current                     # let's wipe out piece current location
        current_piece_x=$1                # update x ...
        current_piece_y=$2                # ... and y of new location
        show_current                      # and draw piece in new location
        return 0                          # nothing more to do here
    fi                                    # if we could not move piece to new location
    (($2 == current_piece_y)) && return 0 # and this was not horizontal move
    process_fallen_piece                  # let's finalize this piece
    get_random_next                       # and start the new one
    return 1
}

cmd_right() {
    move_piece $((current_piece_x + 1)) $current_piece_y
}

cmd_left() {
    move_piece $((current_piece_x - 1)) $current_piece_y
}

cmd_rotate() {
    local available_rotations old_rotation new_rotation

    available_rotations=$((${#piece[$current_piece]} / 8))
    old_rotation=$current_piece_rotation
    new_rotation=$(((old_rotation + 1) % available_rotations))
    current_piece_rotation=$new_rotation
    if new_piece_location_ok $current_piece_x $current_piece_y ; then
        current_piece_rotation=$old_rotation
        clear_current
        current_piece_rotation=$new_rotation
        show_current
    else
        current_piece_rotation=$old_rotation
    fi
}

cmd_down() {
    move_piece $current_piece_x $((current_piece_y + 1))
}

cmd_drop() {
    while move_piece $current_piece_x $((current_piece_y + 1)) ; do : ; done
}

cmd_quit() {
    showtime=false                               # let's stop controller ...
    pkill --full --signal SIGUSR2 "/bin/bash $0" # ... send SIGUSR2 to all script instances to stop forked processes ...
    xyprint $GAMEOVER_X $GAMEOVER_Y "Game over!"
    echo -e "$screen_buffer"                     # ... and print final message
}

controller() {
    # SIGUSR1 and SIGUSR2 are ignored
    trap '' SIGUSR1 SIGUSR2
    local cmd commands

    # initialization of commands array with appropriate functions
    commands[$QUIT]=cmd_quit
    commands[$RIGHT]=cmd_right
    commands[$LEFT]=cmd_left
    commands[$ROTATE]=cmd_rotate
    commands[$DOWN]=cmd_down
    commands[$DROP]=cmd_drop
    commands[$TOGGLE_HELP]=toggle_help
    commands[$TOGGLE_NEXT]=toggle_next

    init

    while $showtime; do           # run while showtime variable is true, it is changed to false in cmd_quit function
        echo -ne "$screen_buffer" # output screen buffer ...
        screen_buffer=""          # ... and reset it
        read -s -n 1 cmd          # read next command from stdout
        ${commands[$cmd]}         # run command
    done
}

stty_g=`stty -g` # let's save terminal state

# output of ticker and reader is joined and piped into controller
(
    ticker & # ticker runs as separate process
    reader
)|(
    controller
)

show_cursor
stty $stty_g # let's restore terminal state
