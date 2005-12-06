echo configuring automake
./bootstrap.sh
echo configuring libiax2
./configure
echo building libiax2
make
echo 
echo
echo '##################################################################'
echo '#                                                                #'
echo '# If all is well, enter "make install" to complete installation. #'
echo '#                                                                #'
echo '##################################################################'
