# Robotika-HW2

# Reakcijos testas 2

Šis projektas yra reakcijos testas, paremtas F1 „penkių lempučių“ principu. Tikslas buvo sukurti realiai veikiantį prototipą su Arduino Uno, naudojant pertraukimus (interrupts), laikmačius (timers) ir EEPROM, be delay() funkcijų.

## Projekto idėja

Įrenginys uždega LED lemputes vieną po kitos kaip lenktynių startas.  
Kai jos visos užgęsta, ekrane pasirodo „GO!“.  
Vartotojas turi kuo greičiau paspausti mygtuką, o LCD ekrane parodomas jo reakcijos laikas.  
Jei mygtukas paspaudžiamas per anksti – pasirodo pranešimas „JUMP START!“  
Visi rezultatai išsaugomi EEPROM, todėl net po maitinimo atjungimo lieka geriausias rezultatas (greičiausias reakcijos laikas) ir bandymų skaičius.

## Komponentai

- 1 x Arduino Uno mikrovaldiklis
- 5 × LED lemputės
- 1 × Mygtukas
- 1 × Garsinis signalizatorius
- 1 × 16×2 LCD ekranas
- 1 x Potenciometras 10K
- 6 x 220Ω rezistoriai
- 1 x Maketo plokštė + laidai

## Kūrimo etapai

1. Prijungti maketo plokštę prie GND ir 5V;
2. Prijungti 5 raudonas LED lemputes: anodas per 220Ω rezistorių į Arduino skaitmeninį (digital) piną (D3-D7), katodas į GND;
3. Prijungti mygtuką įstatant jį į maketo plokšę ir vieną kairįjį apatinį kampą jungiame į D2 Arduino piną, o dešinįjį viršutinį kampą jungiame į GND;
4. Prijungiame garsinį signalizarotių jungdami teigiamą kojelę į D8, o neigiamą kojelę į GND;
5. PRijungiame porenciometrą: kairę kojelę į GND, dešinę – į 5V;
6. Prijungiame 16x2 LCD ekraną:
   - VSS į GND;
   - VDD į 5V;
   - VO su potenciometro vidurine kojele;
   - RS į D12;
   - RW į GND;
   - E į D11;
   - D4 į A0;
   - D5 į A1;
   - D6 į A2;
   - D7 į A3;
   - A per rezistorių į 5V;
   - K į GND;

## Schema
(Žemiau pateikiama schema, kuri taip pat įkelta kaip „schema.png“).
<img width="925" height="657" alt="image" src="https://github.com/user-attachments/assets/32c36e20-0e6b-47fb-93c3-3a8e4af00d55" />

## EEPROM struktūra

Atmintyje išsaugomas geriausias reakcijos laikas "Best" (bestMs) ir bandymų skaičius (attempts). Rezultatai saugomi tik tada, kai kažkas pasikeičia. Naudojant EEPROM rezultatai išlieka net ir atjungus maitinimą. 

## Pertraukimai

Naudojamas Timer1 CTC režimu ir kiekvieną milisekundę siunčiamas signalas programai. Kas 10 ms atliekamas mygtuko „debounce“ tikrinimas, o kas 1 s mirksi LED_BUILTIN (parodo, kad įrenginys „gyvas“).
Išorinis (mygtuko) pertraukimas (D2) informuoja apie pokyčius (paspaudimus).


