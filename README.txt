Onel Alexandru 322CB

Ideea generala a codului:
    Prima data m am uitat prin functii sa vad ce ar merge paralelizat
si mi au sarit in evidenta cele cu for uri imbricate, cele de alocare si 
scriere nu aveau sens sa le paralelizez. Astfel m am gandit ca in Main 
imi voi porni thread urile, care vor merge in functia f() (am pastrat numele
din laborator), acolo vor rula functiile care pot fi rulate in paralel si 
dupa se intorc la join ul din Main si continua cu write() si eliberarea 
de memorie. Astfel primii pasi au fost in Main prin alocarea variabilelor,
am creat niste functii de alocare, acestea sunt pur si simplu codul extras
din functiile secventiale. Dupa ce am alocat, le adaug in fiecare thread in
structura lui specifica, unde se afla si ID ul sau. Dupa aceea, intra
toate in functia f() si ruleaza functiile modificate. Am pus bariere intre ele,
unde datele urmatorei functii depindeau de rezultatul functiei anterioare.
Dupa ce am terminat rularea functiei f() aveam deja tot ce puteam paraleliza 
eficient calculat, urmand join ul thread urilor in Main si dupa write() si 
eliberarea de memorie. Nu am folosit nicio variabila globala, tot ce am avut
nevoie am trimis prin structura ThreadArgs.
    Am paralelizat functiile folosind formula din laborator, cea cu start si end.


            Descrierea procesului gandirii si cum am scris codul pe etape

Cum am implementat paralelizarea: am incercat pas cu pas sa 
paralelizez cate o functie. Astfel prima functie pe care am incercat
sa o paralelizez a fost rescale_image(). Aveam nevoie de imaginea initiala,
deci mi a venit ideea de a imi crea o structura unde sa pasez ce are 
nevoie fiecare thread nou creat. Am pus imaginea intiala, si am mai pus 
inca o imagine, doar alocata deoarece functia initiala avea nevoie de o imagine 
noua alocata. Initial aceasta functie isi aloca in interiorul ei, dar 
am separat intr o alta functie pentru a putea aloca in Main, ca sa nu isi 
aloce fiecare thread aceasta imagine noua. Dupa aceea am adaugat formula din 
laborator(start si end). Ulterior mi am salvat noua imagine in structura mea
pentru a o putea accesa din main(pentru a continua algoritmul secvential cu noua 
imagine scalata). Dupa ce am rezolvat probleme de segm fault, la rularea 
cu dockerul am scos timpi foarte buni, paralelizarea rescale_image() imi 
treceau testele de scalabilitate uneori. Am continuat cu urmatoarea functie: 
sample_grid(), dar inainte de ea a trebuit sa pun o bariera deoarece aveam 
nevoie de scaled_image, aici se pierde putin timp din cauza barierei. Am alocat 
memoria cu ajutorul functiei de allocMemoryForNewGrid() in Main, am pus o in structura
pentru informatiile thread ului si am paralelizat functia sample_grid() prin 
adaugarea formulei de la laborator(start si end). Gridul calculat a ramas salvat 
in argumentele thread urilor si puteam accesa si din main, oricum aveam pointerul in main
pentru toate(imagine, grid, contour_map). Identic si pentru functia march(). Astia au fost 
pasii pe care i am urmat.

Feedback:
    A fost o tema destul de usoara, personal mi a luat cam 2 zile(nu chiar full),
asta din cauza unor buguri. Modelul acesta de tema mi se pare si util deoarece arata
cat poti eficientiza ca timp un program usor paralelizabil.
