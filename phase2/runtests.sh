#!/bin/bash

array=($(ls testcases))
fname="results"
resultsdir="testResults/"
myresultsdir="myResults/"
fext=".txt"
difftext="diff"
maxtest=44
diffdir="diffOutputs/"

rm myResults/* &> /dev/null
rm diffOutputs/* &> /dev/null
mkdir myResults &> /dev/null
mkdir diffOutputs &> /dev/null


count=0

for i in "${array[@]}"
do
    test="$(cut -d'.' -f1 <<< $i)"

    if [ $test == "term0" ] || [ $test == "term1" ] || [ $test == "term2" ] || [ $test == "term3" ]; then
        echo "$test is not a testcase, skipping...."
    else

        echo -n "Running $test ....................... "

        make $test &> /dev/null

        eval ./$test &> $myresultsdir$test$fname$fext
        diffresults="$(diff $resultsdir$test$fext $myresultsdir$test$fname$fext)"
        diffsize=${#diffresults}

        if [ $diffsize -gt 0 ]; then
            echo "FAILED"
            diff $resultsdir$test$fext $myresultsdir$test$fname$fext &> $diffdir$test$difftext$fext
        else
            echo "SUCCEEDED"
        fi

        if [ $count -gt $(expr $maxtest - 1) ];  then
            break
        fi

        count=$(expr $count + 1)
    fi
done

make clean &> /dev/null
