# Author: bdgoocci@gmail.com
# Usage: ./split.sh file size   
# file为目标分割文件路径，size为切割后单个文件大小，单位M
# Description: split.sh：循环分割文件，结果自动存在result下，分割后的文件以数字结尾，例如file分割为两个文件：file1和file2.

#!/bin/sh

#使用脚本是第一参数是要分割的文件名
allFiles=${*%${!#}}
Filesize=0
SubFilesize=${!#}
Fileline=0
SubFileline=0
SubFilenum=0
Path=`pwd`

#echo "DebugInfo:"
#numArgs="$#"
#echo "Number of args: $numArgs"
#firstArg="$1"
#echo "First arg: $firstArg"
#lastArg="${!#}"
#echo "Last arg: $lastArg"echo "DebugInfo: t"
#allArgs="$@"
#echo "$allFiles"
#other=${*%${!#}}
#echo "$other"

##验证文件名是否正确
#if [ -z $Filename ];then
#    echo "Error:The file name can not be empty"
#    exit
#fi

#检验是否输入分割文件后的大小
if [ $SubFilesize == 0 ];then
    echo "Error: The SubFilesize haven't been pointed"
    exit
fi

#处理传入的是directory的情况，只处理一级directory
count=0
echo "$allFiles"
for allFile in $allFiles
    do
        Files=$allFile
        Name=$allFile
        if [ -d "$Name" ];then
            cd $Name
            Files=`find *`
        fi
        echo "Debug: current processing file"
        echo "$Name"
        echo "$Files"

        for file in $Files
            do
                Filename=$file
                #一些准备工作
                mkdir $Filename"_result"
                #将目标文件拷贝到result,局限于split只会将分割文件放在当前目录
                cp $Filename $Filename"_result"
                if [ -e $Filename ];then
                    Fileline=`wc -l $Filename | awk '{print $1}'`
                    Filesize=`wc -c $Filename | awk '{print $1}'`
                    if [ $Fileline == 0 ];then
                    echo "Error:The File line is zero!"
                    exit
                    fi
                    if [ $Filesize == 0 ];then
                    echo "Error:The File size is zero!"
                    exit
                    fi
                    echo "总共有 $Fileline 行"
                    echo "总共有 `expr $Filesize / 1024 / 1024` M:"
                else
                    echo "Error:$Filename does not exist!"
                    exit 
                fi

                echo $SubFilesize | grep '^[0-9]\+$' >> /dev/null
                if [ $? -ne 0 ];then
                    echo "Error:The SubFilesize is not a number!"
                    exit
                elif [ $SubFilesize -eq 0 ];then
                    echo "Error:The SubFilesize line is zero!"
                    exit
                fi

                #根据文件大小和期望分割后子文件的大小，总行数，确定每个子文件有多少行
                #Filesize/SubFilesize=Fileline/SubFileline
                #这种分割方法只是初略按照行数划分，因为每行的大小未必相同，
                #所以结果只能保证子文件大小和设定大小尽量接近，而非准确相等
                SubFileline=`expr $Fileline \* $SubFilesize \* 1024 \* 1024 / $Filesize`
                #计算需要分割为几个文件
                #SubFilenum=`expr $Fileline / $SubFileline`
                #计算余数
                Remainder=`expr $Fileline % $SubFileline`
                if [ $Remainder > 0 ];then
                    SubFilenum=`expr $Fileline / $SubFileline + 1`
                else
                    SubFilenum=`expr $Fileline / $SubFileline`
                fi

                check=1
                if [ $count == 0 ];then
                    echo "分割后将会有 $SubFilenum 文件, 每个文件大小 $SubFilesize M, 每个文件 $SubFileline 行"

                    echo "是否继续(1/0)？(提示：分割后文件太多将导致分割失败,建议手动多次分割)"
                    read check
                fi
                count=1

                if [ $check == 1 ];then
                    cd $Filename"_result"
                    #将文件分割
                    split -l $SubFileline $Filename -d -a 2 $Filename"_"
                    #将临时复制的文件删掉并且退出result目录
                    rm -rf $Filename && cd ..
                else 
                    rm -rf $Filename"_result"
                    echo "退出，请重新运行脚本"
                    exit
                fi
            done
    done
