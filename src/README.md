**Nume: Petrea Octavian**

**Grupa: 333CA**

                Tema 1 - Algoritmi paraleli si distribuiti
        Calculul paralel al unui index inversat folosind paradigma Map-Reduce


**SCOP**


Această implementare are ca scop realizarea unui index inversat folosind
paradigma Map-Reduce.


**DESCRIERE**

Programul folosește o structură de context partajată (MappData) care conține o
coadă pentru gestionarea fișierelor ce trebuie procesate, mutex-uri pentru
sincronizarea thread-urilor, o barieră pentru coordonarea între fazele mapper și
reducer, un array de rezultate pentru stocarea datelor intermediare și contoare
pentru urmărirea progresului. 

Totodata, programul este impartit in 2 faze: faza Mapper si faza Reduccer:

1. Faza Mapper:
Thread-urile mapper se ocupă de procesarea inițială a fișierelor. Fiecare thread
mapper preia fișiere dintr-o coadă partajată până când toate fișierele sunt
procesate. Pentru fiecare fișier, mapper-ul convertește toate cuvintele la
litere mici, elimină caracterele non-alfabetice, creează un set de cuvinte
unice pentru a evita duplicatele și asociază fiecare cuvânt cu indexul
fișierului. Rezultatele sunt stocate în vectori specifici fiecărui thread
pentru a minimiza conflictele.

2. Faza Reducer:
Thread-urile reducer efectuează agregarea și sortarea. Fiecare reducer este
responsabil pentru un interval specific de litere de început și procesează
cuvintele care încep cu literele atribuite. Rezultatele sunt organizate într-o
structură de tip map unde cheia primară este litera de început, cheia secundară
este cuvântul în sine, iar valoarea este un set de indici ai fișierelor unde
apare cuvântul. Rezultatele sunt sortate după frecvența de apariție a cuvântului
(descrescător) și alfabetic în cazul frecvențelor egale.

 

