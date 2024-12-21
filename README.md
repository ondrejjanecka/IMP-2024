# IMP-2024

**Ondřej Janečka (xjanec33)**

## Automatické zalévání rostlin v interiéru

### Popis

Předmětem projektu je vytvoření automatického systému zalévání rostlin kombinací analogového senzoru vlhkosti půdy a vodního čerpadla. Úroveň vlhkosti půdy se zobrazí na displeji a pomocí MQTT bude systém data publikovat na MQTT broker. Příjmem MQTT zpráv bude možné nastavit minimální vlhkost a hraniční vlhkost, kterou je nutné překročit při zalévání (jako záložní možnost bude možnost nastavení pomocí tlačítka). Tlačítko bude přepínat mezi módy automatické funkce a přípravného režimu.

### Realizační základna

vývojový kit založený na ESP32 s OLED displejem, analogový sensor vlhkosti, vodní čerpadlo, tlačítko

### Požadavky řešení

#### Povinné

Vytvoření systému, který bude v pravidelných intervalech kontrolovat vlhkost půdy, při nízké vlhkosti spustí v cyklech čerpadlo, dokud se vlhkost nedostane nad přednastavenou hodnotu; zobrazování hodnot na displeji a nastavení hraničních hodnot. Komunikace s MQTT brokerem.

#### Volitelné

Připojení a synchronizace času z NTP serveru pro deaktivaci zalévání v noci.

#### Mimořádné

Žádné

## Hodnocení: 14/14 bodů

| Kritérium                                                                                              | Body     |
|--------------------------------------------------------------------------------------------------------|----------|
| Chvályhodné nadšení pro řešení projektu, snaha řešit projekt včas, kvalitně a/nebo nad rámec zadání    | **2/2**  |
| Řešení je zcela funkční                                                                                | **5/5**  |
| Způsob řešení je pochopitelný a přehledný i bez čtení komentářů/dokumentace                            | **1/1**  |
| Uživatelská přívětivost řešení je na vysoké úrovni                                                    | **1/1**  |
| Úvod do problematiky (motivace, přehled použitých HW/SW prostředků, principy, protokoly, veličiny atd.) v dokumentaci je zpracován chvályhodně | **1/1**  |
| Popis způsobu řešení v dokumentaci je zpracován chvályhodně                                           | **1/1**  |
| Zhodnocení řešení v dokumentaci je provedeno chvályhodně                                              | **1/1**  |
| Prezentace řešení byla chvályhodná                                                                    | **2/2**  |

### Celkem: **14/14 bodů**

#### Doplňující komentář

Prezentováno formou videa na vlastním vybavení. Dokumentace mohla být více ilustrativní (schéma propojení komponent apod.), ale tento nedostatek vyvažují její jiné nadprůměrné kvality.
