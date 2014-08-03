Ocelot for torrentpier
==================
###Версия трекера Ocelot (github.com/WhatCD/) под фронтенд в виде Torrentpier (torrentpier.me)
## Текущие недоработки:
1) Отданное релизером больше не засчитывается в "На своих"  -  Будет исправлено

2) Недостает некоторых cron-jobs в аннонсере (нет нужды для задач во встроенном в двиг планировщике).

3) Присылайте найденные баги.

Работоспособность проверена на Debian 7 

Инструкция по сборке: https://github.com/WhatCD/Gazelle/wiki/Gazelle-installation-on-Ubuntu-14.04 
На шаге Configure, Compile, Install следует указывать 
> ./configure --with-mysql-lib=/usr/lib/ --with-ev-lib=/usr/lib/ 


