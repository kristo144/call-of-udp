# Descripció protocol Call of UDP

Aquest document descriurà la comunicació proposada entre servidor i client del joc.

## Descripció del joc

Dos jugadors es connecten al servidor.
Es troben en una graella de 8x8 amb parets i espais en blanc.
Cada jugador espera el seu torn, i llavors pot realitzar una acció.

Les accions són moure's, girar-se, o disparar;
i les pot intentar fer en qualsevol de les quatre direccions.
No es pot moure dins una paret.
Les accions de moure i disparar també fan girar el jugador a la mateixa direcció.

Cada jugador només veu el que té davant, i no a través de parets.
El jugador que dispari a l'altre guanya.

## Format

Cada datagrama contindrà exactament una comanda.
Totes les comandes es codifiquen en sèries d'octets ASCII terminades amb la sequencia `\n\0`.
Tota la informació numèrica s'envia en format decimal.

La cookie de sessió serà un nombre aleatori de 16 bits.
Més avall es representarà com `{C}`.

## Missatges del client

| Missatge           | Mnemonic                           | Descripcio                                |
| ---                | ---                                | ---------                                 |
| `R`                | Request                            | El client demana una cookie del servidor. |
| `{C}V`             | View                               | El client demana veure el mapa.           |
| `{C}[MTS][UDLR]` | Move/Turn/Shoot Up/Down/Left/Right | El client vol realitzar una accio.        |

## Missatges del servidor

| Missatge | Mnemonic       | Descripcio                                                                                                         |
| ---      | ---            | -------------------                                                                                                |
| `{C}`    | Cookie         | El servidor assigna una cookie al client.                                                                          |
| `F`      | Full           | El servidor rebutja la conexio del client.                                                                         |
| `M:n:d`  | Map            | Mostra el mapa al client. `n` serà el nombre de columnes en format decimal. `d` seran les dades literals del mapa. |
| `K`      | oK             | L'acció del client és vàlida.                                                                                      |
| `I`      | Invalid        | L'acció del client és invalida.                                                                                    |
| `E`      | Error          | La petició del client no és correcta.                                                                              |
| `W`      | Win            | El client ha guanyat.                                                                                              |
| `L`      | Loss           | El client ha perdut.                                                                                               |
| `T`      | your Turn      | És el torn del client i pot fer una acció.                                                                         |
| `N`      | Not your turn  | El client vol fer una acció quan no és el seu torn.                                                                |
| `U`      | Unknown cookie | La cookie donada pel client no és reconeguda.                                                                      |

\newpage

## Format del mapa

| Caracter  | Significat                       |
| ---       | ---                              |
| `X`       | Paret                            |
| (espai)   | Casella lliure                   |
| `< ^ > v` | Jugador en diverses orientacions |

## Flux del joc proposat

- El client demana una cookie al servidor.
- Si el servidor té espai, assigna una cookie al client.
- El client espera el seu torn.
- El servidor comunica al client si ha guanyat, si ha perdut, o si és el seu torn.
- Si és el torn del client, demana veure el mapa.
- El servidor mostra el mapa al client.
- El client intenta fer una acció i espera una resposta. Si no és vàlida, torna a intentar fer una acció.
- Si l'acció ha estat valida, tornar al pas de esperar el seu torn.
