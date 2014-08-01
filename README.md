Ocelot for torrentpier
==================
###Очень ранняя версия трекера Ocelot (github.com/WhatCD/) под фронтенд в виде Torrentpier (torrentpier.me)
## Текущие недоработки:
1) Отданное релизером больше не засчитывается в "На своих"  -  Не будет исправлено

2) Файлы интеграции для торрентпира (config.php, functions_torrent.php) еще не готовы для выкладывания.

3) Присылайте найденные баги.

Требует средних по уровню сложности изменений в двиге. Будут выкладываться только патчи на файлы (измененный код).

Без знания php не советую устанавливать данный трекер.
Работоспособность проверена на Debian 7

Инструкция по сборке: https://github.com/WhatCD/Gazelle/wiki/Gazelle-installation-on-Ubuntu-14.04 
На шаге Configure, Compile, Install следует указывать 
> ./configure --with-mysql-lib=/usr/lib/ --with-ev-lib=/usr/lib/ 


