#!/bin/bash

function failwith() {
    echo -ne "\e[1;31m[ERROR]\e[0m "
    echo "$1"
    exit 1
}

function test_tokenize() {
    res=$(echo "$1" | ./main tokenize)
    if [ "$res" != "$1" ]; then
        failwith "tokenize test error: $1"
    fi
}

test_tokenize "{\"hoge\":\"piyo\",\"foo\":-123,\"fuga\":[-45,62,7]}"

function test_parse() {
    if ruby -r json -e "\
        if JSON.parse(open(ARGV[0]).read) != JSON.parse(open(ARGV[1]).read)
            exit(1)
        end" -- <(echo "$1" | ./main parse) <(echo "$1"); then
        :   # OK
    else
        failwith "parse test error: $1"
    fi
}

test_parse '{"hoge": "piyo", "foo": -123, "fuga": [-45, [62], {"so": 7}]}'
