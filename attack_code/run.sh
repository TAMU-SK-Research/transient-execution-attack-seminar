echo $#
if [ $# != 1 ]
then
    echo "Usage: $0 spectre_v#";
    exit;
fi

../build/X86_MESI_Two_Level/gem5.opt \
    ../configs/example/se.py \
    --cpu-type=DerivO3CPU \
    --ruby \
    --cmd=./$1
