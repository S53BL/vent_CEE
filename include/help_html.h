#ifndef HELP_HTML_H
#define HELP_HTML_H

#include "html.h"

// help_html.h - HTML za stran /help
// Nav bar se vključi iz html.h (HTML_NAV_CSS + HTML_NAV_BAR)
// Vsebina (help-container) z sticky kazalom in izboljšanim iskanjem

const char* HTML_HELP = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>Pomoč - Ventilacijski sistem</title>
    <style>
        *{box-sizing:border-box;margin:0;padding:0}
        body{font-family:Arial,sans-serif;font-size:15px;background:#101010;color:#e0e0e0;}
        nav{background:#1a1a1a;border-bottom:1px solid #333;padding:8px 16px;display:flex;gap:8px;flex-wrap:wrap;align-items:center;box-shadow:0 2px 8px rgba(0,0,0,.4);position:sticky;top:0;z-index:1000;}
        .nav-home{background:#4caf50;color:#fff;border-radius:5px;padding:7px 11px;font-size:17px;text-decoration:none;line-height:1;display:inline-block;}
        .nav-home:hover{background:#66bb6a;}
        .nav-btn{background:#2a2a2a;color:#4da6ff;border:1px solid #4da6ff;border-radius:5px;padding:7px 14px;font-size:13px;font-weight:bold;text-decoration:none;white-space:nowrap;}
        .nav-btn:hover,.nav-btn.current{background:#4da6ff;color:#101010;}
        .nav-sep{width:1px;background:#444;align-self:stretch;margin:0 4px;}
        .nav-ext{background:#2a2a2a;color:#aaa;border:1px solid #444;border-radius:5px;padding:7px 14px;font-size:13px;font-weight:bold;text-decoration:none;white-space:nowrap;}
        .nav-ext:hover{background:#333;color:#e0e0e0;border-color:#666;}
        .wrap{max-width:1400px;margin:0 auto;padding:16px;display:flex;gap:20px;}
        h1{font-size:22px;color:#e0e0e0;margin:16px 0 20px 0;text-align:center;}
        
        /* Sticky Table of Contents */
        .toc-container{width:280px;flex-shrink:0;}
        .toc{background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:15px;position:sticky;top:80px;max-height:calc(100vh - 100px);overflow-y:auto;}
        .toc h2{font-size:16px;color:#4da6ff;margin-bottom:10px;padding-bottom:8px;border-bottom:1px solid #333;}
        .toc ul{list-style:none;padding:0;}
        .toc li{margin:6px 0;}
        .toc a{color:#aaa;text-decoration:none;font-size:13px;display:block;padding:4px 8px;border-radius:4px;transition:all 0.2s;}
        .toc a:hover{background:#2a2a2a;color:#4da6ff;}
        .toc a.active{background:#4da6ff;color:#101010;font-weight:bold;}
        .toc .toc-subsection{padding-left:12px;font-size:12px;}
        
        /* Main content */
        .content-container{flex:1;min-width:0;}
        .search-bar{display:flex;gap:8px;margin-bottom:20px;align-items:center;flex-wrap:wrap;}
        .search-input{flex:1;min-width:200px;padding:8px 10px;background:#2a2a2a;border:1px solid #555;border-radius:4px;color:#e0e0e0;font-size:13px;}
        .search-input:focus{outline:none;border-color:#4da6ff;}
        .search-button{padding:8px 14px;background:#4da6ff;color:#101010;border:none;border-radius:4px;cursor:pointer;font-size:13px;font-weight:bold;}
        .search-button:hover{background:#6bb3ff;}
        .search-nav-btn{padding:6px 12px;background:#2a2a2a;color:#4da6ff;border:1px solid #4da6ff;border-radius:4px;cursor:pointer;font-size:12px;font-weight:bold;}
        .search-nav-btn:hover{background:#4da6ff;color:#101010;}
        .search-nav-btn:disabled{opacity:0.5;cursor:not-allowed;}
        .search-results{font-size:12px;color:#aaa;margin-left:4px;}
        
        .help-container{background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:20px;box-shadow:0 2px 8px rgba(0,0,0,.3);}
        h2{color:#4da6ff;margin-top:30px;margin-bottom:10px;padding-top:20px;border-top:1px solid #333;}
        h2:first-child{margin-top:0;padding-top:0;border-top:none;}
        h3{color:#4da6ff;margin-top:20px;margin-bottom:8px;}
        h4{color:#6bb3ff;margin-top:15px;margin-bottom:5px;}
        p{margin-bottom:15px;line-height:1.6;}
        ul,ol{margin-bottom:20px;padding-left:20px;}
        li{margin-bottom:8px;line-height:1.5;}
        table{width:100%;border-collapse:collapse;margin-bottom:20px;background:#252525;border-radius:4px;overflow:hidden;}
        th,td{padding:12px;text-align:left;border-bottom:1px solid #333;}
        th{background-color:#2a2a2a;color:#e0e0e0;font-weight:bold;}
        tr:hover{background-color:#2a2a2a;}
        strong{color:#4da6ff;}
        hr{border:none;border-top:1px solid #333;margin:30px 0;}
        em{color:#aaa;font-style:italic;}
        
        /* Search highlight */
        .highlight{background-color:#ff9800;color:#000;padding:2px 4px;border-radius:2px;}
        .highlight.current{background-color:#ffeb3b;box-shadow:0 0 8px #ffeb3b;}
        
        /* Responsive */
        @media (max-width: 900px) {
            .wrap{flex-direction:column;}
            .toc-container{width:100%;}
            .toc{position:relative;top:0;max-height:none;}
        }
    </style>
</head>
<body>
    <nav>
        <a href='/' class='nav-home' title='CEE domača stran'>&#127968;</a>
        <a href='/' class='nav-btn'>Domača</a>
        <a href='/settings' class='nav-btn'>Nastavitve</a>
        <a href='/help' class='nav-btn'>Pomoč</a>
        <span class='nav-sep'></span>
        <a href='http://192.168.2.190/' class='nav-ext'>REW</a>
        <a href='http://192.168.2.193/' class='nav-ext'>UT</a>
        <a href='http://192.168.2.194/' class='nav-ext'>KOP</a>
        <a href='http://192.168.2.195/' class='nav-ext'>SEW1</a>
        <a href='http://192.168.2.196/' class='nav-ext'>SEW2</a>
        <a href='http://192.168.2.197/' class='nav-ext'>SEW3</a>
        <a href='http://192.168.2.198/' class='nav-ext'>SEW4</a>
        <a href='http://192.168.2.199/' class='nav-ext'>SEW5</a>
    </nav>
    <script>
        document.addEventListener('DOMContentLoaded',function(){
            var p=window.location.pathname.replace(/\/$/,'')||'/';
            document.querySelectorAll('.nav-btn').forEach(function(b){
                if(b.getAttribute('href')===p)b.classList.add('current');
            });
        });
    </script>

    <h1>Pomoč - Ventilacijski sistem</h1>

    <div class="wrap">
        <!-- Sticky Table of Contents -->
        <div class="toc-container">
            <div class="toc">
                <h2>Kazalo vsebine</h2>
                <ul>
                    <li><a href="#1-uvod-v-sistem">1. Uvod v sistem</a>
                        <ul class="toc-subsection">
                            <li><a href="#1-1-kaj-je-ventilacijski-sistem">1.1 Kaj je ventilacijski sistem?</a></li>
                            <li><a href="#1-2-katere-prostore-sistem-nadzira">1.2 Katere prostore sistem nadzira?</a></li>
                        </ul>
                    </li>
                    <li><a href="#2-razumevanje-spletne-strani">2. Razumevanje spletne strani</a>
                        <ul class="toc-subsection">
                            <li><a href="#2-1-domaca-stran">2.1 Domača stran</a></li>
                            <li><a href="#2-2-stran-z-nastavitvami">2.2 Stran z nastavitvami</a></li>
                            <li><a href="#2-3-stran-za-kalibracijo-senzorjev">2.3 Stran za kalibracijo senzorjev</a></li>
                        </ul>
                    </li>
                    <li><a href="#3-kako-delujejo-ventilatorji">3. Kako delujejo ventilatorji</a>
                        <ul class="toc-subsection">
                            <li><a href="#3-1-trije-nacini-krmiljenja">3.1 Trije načini krmiljenja</a></li>
                            <li><a href="#3-2-wc-ventilator">3.2 WC ventilator</a></li>
                            <li><a href="#3-3-kopalnica-ventilator">3.3 Kopalnica ventilator</a></li>
                            <li><a href="#3-4-utility-ventilator">3.4 Utility ventilator</a></li>
                            <li><a href="#3-5-dnevni-prostor">3.5 Dnevni prostor</a></li>
                            <li><a href="#3-6-skupni-vhod">3.6 Skupni vhod</a></li>
                        </ul>
                    </li>
                    <li><a href="#4-poseben-program-za-susenje">4. Poseben program za sušenje</a>
                        <ul class="toc-subsection">
                            <li><a href="#4-1-kaj-je-poseben-program">4.1 Kaj je poseben program za sušenje?</a></li>
                            <li><a href="#4-2-kako-deluje-v-kopalnici">4.2 Kako deluje v kopalnici?</a></li>
                            <li><a href="#4-3-kako-deluje-v-utilityju">4.3 Kako deluje v utilityju?</a></li>
                            <li><a href="#4-4-kaj-vidite-na-spletni-strani">4.4 Kaj vidite na spletni strani?</a></li>
                            <li><a href="#4-5-kako-prilagodite-program">4.5 Kako prilagodite program?</a></li>
                            <li><a href="#4-6-posebni-primeri">4.6 Posebni primeri</a></li>
                        </ul>
                    </li>
                    <li><a href="#5-nocni-in-dnevni-cas">5. Nočni in dnevni čas – omejitve</a>
                        <ul class="toc-subsection">
                            <li><a href="#5-1-nocni-cas">5.1 Nočni čas (22:00–06:00)</a></li>
                            <li><a href="#5-2-ko-ni-nikogar-doma">5.2 Ko ni nikogar doma (06:00–16:00)</a></li>
                        </ul>
                    </li>
                    <li><a href="#6-vpliv-zunanjega-vremena">6. Vpliv zunanjega vremena</a></li>
                    <li><a href="#7-nastavitve">7. Nastavitve – kako prilagoditi</a>
                        <ul class="toc-subsection">
                            <li><a href="#7-1-splosne-nastavitve">7.1 Splošne nastavitve</a></li>
                            <li><a href="#7-2-nastavitve-za-dnd">7.2 Nastavitve za DND</a></li>
                            <li><a href="#7-3-nastavitve-za-dnevni-prostor">7.3 Nastavitve za Dnevni prostor</a></li>
                            <li><a href="#7-4-stopnje-ventilatorjev">7.4 Stopnje ventilatorjev v dnevnem prostoru</a></li>
                            <li><a href="#7-5-natancnejsa-razlaga-nnd">7.5 Natančnejša razlaga NND</a></li>
                            <li><a href="#7-6-primeri-prilagajanja">7.6 Primeri prilagajanja</a></li>
                            <li><a href="#7-7-kako-shraniti-nastavitve">7.7 Kako shraniti nastavitve</a></li>
                        </ul>
                    </li>
                </ul>
            </div>
        </div>

        <!-- Main Content -->
        <div class="content-container">
            <div class="search-bar">
                <input type="text" id="searchInput" class="search-input" placeholder="Iskanje po vsebini...">
                <button type="button" id="searchButton" class="search-button">Išči</button>
                <button type="button" id="prevButton" class="search-nav-btn" disabled>◄ Prejšnji</button>
                <button type="button" id="nextButton" class="search-nav-btn" disabled>Naslednji ►</button>
                <span id="searchResults" class="search-results"></span>
            </div>

            <div class="help-container">
                <p>Dobrodošli v pomoč za vaš ventilacijski sistem! Ta priročnik vam pomaga razumeti, kako sistem deluje in kako ga prilagodite svojim potrebam. Sistem samodejno skrbi za svež zrak v vašem domu, ampak vi lahko nadzirate in spreminjate njegovo delovanje prek spletne strani.</p>

                <p>Če odprete spletno stran sistema (običajno na naslovu kot 192.168.2.192), vidite pregled stanja. Kliknite "Pomoč" na vrhu strani, da pridete sem.</p>

                <p>V tem priročniku boste našli:</p>
                <ul>
                    <li>Kako sistem prezračuje različne prostore v vašem domu.</li>
                    <li>Kaj sproži ventilatorje (ročno, samodejno ob vaših akcijah ali popolnoma samodejno).</li>
                    <li>Kako prilagodite nastavitve na strani "/settings".</li>
                    <li>Nasvete, če se vam zdi prezračevanje prekratko, preglasno ali neustrezno.</li>
                </ul>

                <p>Vse je pojasnjeno preprosto, s primeri iz vsakdanjega življenja. Brez tehničnih izrazov. Če ne želite brati vsega, uporabite kazalo ali iskanje (npr. "kopalnica" za informacije o kopalnici).</p>

                <hr>

                <h2 id="1-uvod-v-sistem">1. Uvod v sistem</h2>

                <h3 id="1-1-kaj-je-ventilacijski-sistem">1.1 Kaj je ventilacijski sistem?</h3>
                <p>Sistem prezračuje vaš dom avtomatsko: meri vlago, temperaturo in CO2, da ohranja svež zrak brez ročnega poseganja. Deluje na tri načine – ročno (vi vklopite), polavtomatsko (reagira na vaše akcije, npr. ugasnjeno luč) ali avtomatsko (sam zazna visoko vlago). To pomeni manj vlage v kopalnici po prhanju, boljši zrak v dnevnem prostoru med kuhanjem in manj hrupa ponoči.</p>

                <p>Spletna stran je glavni način upravljanja: na domači strani vidite trenutne podatke, na "Nastavitve" prilagajate parametre. Če ste novi, začnite z branjem razdelka 2 za opis strani.</p>

                <h3 id="1-2-katere-prostore-sistem-nadzira">1.2 Katere prostore sistem nadzira?</h3>
                <p>Sistem upravlja štiri cone:</p>

                <table>
                    <thead>
                        <tr>
                            <th>Prostor</th>
                            <th>Opis</th>
                            <th>Glavna naloga</th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr>
                            <td><strong>WC</strong></td>
                            <td>Stranišče</td>
                            <td>Prezračevanje po uporabi (večinoma polavtomatsko)</td>
                        </tr>
                        <tr>
                            <td><strong>Kopalnica</strong></td>
                            <td>Kopalnica</td>
                            <td>Odstranjevanje vlage po prhanju (avtomatsko + ročno)</td>
                        </tr>
                        <tr>
                            <td><strong>Utility</strong></td>
                            <td>Pralnica/shramba</td>
                            <td>Prezračevanje ob vlagi ali delu (avtomatsko + ročno)</td>
                        </tr>
                        <tr>
                            <td><strong>Dnevni prostor</strong></td>
                            <td>Bivalni prostor</td>
                            <td>Stalno ohranjanje zraka (ciklično, glede na CO2/vlago)</td>
                        </tr>
                    </tbody>
                </table>

                <h2 id="2-razumevanje-spletne-strani">2. Razumevanje spletne strani</h2>
                <p>Spletna stran je preprosta: domača stran prikazuje trenutno stanje, strani "Nastavitve" in "Nastavitve senzorjev" pa za prilagajanje. Če odprete stran v brskalniku (npr. na telefonu ali računalniku v domačem omrežju), vidite naslednje.</p>

                <h3 id="2-1-domaca-stran">2.1 Domača stran</h3>
                <p>To je glavni zaslon z okni (karticami) za vsak prostor in sistem. Vsako okno kaže status v realnem času.</p>

                <h4>Statusi in barve</h4>
                <ul>
                    <li><strong>ON / VKLOPLJEN</strong>: Ventilator dela.</li>
                    <li><strong>OFF / IZKLOPLJEN</strong>: Ventilator miruje.</li>
                    <li><strong>DISABLED / ONEMOGOČEN</strong>: Ročno izklopljen (preko spleta ali stikala).</li>
                    <li><strong>DRYING 1/2/3</strong>: Ventilator v sušilnem ciklu (1=lahki, 2=srednji, 3=močni). Ventilator periodično dela in miruje, da učinkovito odstrani vlago.</li>
                    <li><strong>Preostali čas</strong>: Šteje nazaj sekunde do izklopa ventilatorja (ali do konca drying cikla).</li>
                </ul>

                <h4>Okna na domači strani:</h4>
                <ul>
                    <li><strong>WC</strong>: Kaže, ali je luč prižgana, ali ventilator dela in preostali čas.</li>
                    <li><strong>Kopalnica</strong>: Prikazuje temperaturo/vlago, status tipke/luči in ventilatorja.</li>
                    <li><strong>Utility</strong>: Podobno kot kopalnica, plus status stikala in luči.</li>
                    <li><strong>Dnevni prostor</strong>: Temperatura, vlaga, CO2; status oken/vrat; stopnja ventilatorja (0-3) in odstotek aktivnosti cikla.</li>
                    <li><strong>Zunanji podatki</strong>: Zunanja temperatura/vlaga/tlak; ali je DND (noč) ali NND (ni doma).</li>
                    <li><strong>Napajanje</strong>: Napetosti, poraba energije.</li>
                    <li><strong>Sistem</strong>: Uporaba RAM-a, uptime, log velikost.</li>
                    <li><strong>Status</strong>: Ali senzorji delujejo, napake.</li>
                </ul>

                <p>Na vrhu strani so povezave: "Nastavitve" (za prilagajanje parametrov), "Nastavitve senzorjev" (če odčitki niso točni), "Pomoč" (ta priročnik).</p>

                <h3 id="2-2-stran-z-nastavitvami">2.2 Stran z nastavitvami (Nastavitve)</h3>
                <p>Tu prilagajate parametre. Vsak parameter ima polje za vrednost, opis in obseg. Spremenite vrednost, kliknite "Shrani" – sprememba velja takoj. Če ne veste, kaj parameter naredi, glej razdelek 7 tega priročnika, kjer so vsi razloženi s primeri.</p>

                <h3 id="2-3-stran-za-kalibracijo-senzorjev">2.3 Stran za kalibracijo senzorjev (Nastavitve senzorjev)</h3>
                <p>Če senzorji kažejo napačno temperaturo/vlago (npr. +1°C preveč), tu dodate popravek (offset). Sistem omogoča popravek za vsak senzor posebej:</p>
                <ul>
                    <li><strong>Popravek temperature BME280</strong> (kopalnica): Če termometer v kopalnici kaže 23°C, senzor pa 24°C, nastavite offset -1.0°C.</li>
                    <li><strong>Popravek vlage BME280</strong> (kopalnica): Če higrometer kaže 50%, senzor pa 55%, nastavite offset -5.0%.</li>
                    <li><strong>Popravek temperature SHT41</strong> (utility): Podobno kot za kopalnico.</li>
                    <li><strong>Popravek vlage SHT41</strong> (utility): Podobno kot za kopalnico.</li>
                    <li><strong>Popravek tlaka BME280</strong>: Redko potreben.</li>
                </ul>
                <p>Vnesite vrednost, kliknite "Shrani" – senzorji se popravijo takoj. Uporabite, če primerjate z drugim natančnim merilnim pripomočkom. Sistem že vključuje nekaj prednastavljenih offsetov za kompenzacijo tipičnih odstopanj senzorjev.</p>

                <h2 id="3-kako-delujejo-ventilatorji">3. Kako delujejo ventilatorji</h2>
                <p>Sistem ima tri načine krmiljenja za vsak prostor: ročni (vi ukrepate), polavtomatski (reagira na vaše akcije) in avtomatski (sam meri). Vsak prostor ima prilagojeno logiko. Tukaj so primeri, kdaj se ventilator vklopi/izklopi, in kako to prilagajate v nastavitvah.</p>

                <h3 id="3-1-trije-nacini-krmiljenja">3.1 Trije načini krmiljenja</h3>
                <ul>
                    <li><strong>Ročni</strong>: Vi pritisnete tipko, stikalo ali gumb. Primer: Pred odhodom pritisnete tipko v kopalnici – ventilator dela določen čas.</li>
                    <li><strong>Polavtomatski</strong>: Sistem zazna vašo akcijo, npr. ugasnjeno luč. Primer: Ko v WC-ju ugasnete luč – se ventilator vklopi avtomatsko.</li>
                    <li><strong>Avtomatski</strong>: Sistem meri vlago/CO2 in ukrepa sam. Primer: Po prhanju vlaga naraste – ventilator se vklopi brez vašega posega.</li>
                </ul>

                <h3 id="3-2-wc-ventilator">3.2 WC ventilator</h3>
                <p><strong>Glavna logika:</strong> Polavtomatski + ročni. Brez avtomatskega po vlagi.</p>
                <ul>
                    <li><strong>Vklop (polavtomatski)</strong>: Ko ugasnete luč (prehod ON → OFF). Ventilator dela določen čas (glej parameter "Čas delovanja ventilatorjev po sprožilcu" v nastavitvah – če se vam zdi prekratek, ga povečajte).</li>
                    <li><strong>Vklop (ročni)</strong>: Preko naprave v dnevni sobi ali stikala.</li>
                    <li><strong>Izklop</strong>: Po času ali ročno.</li>
                    <li><strong>Posebno</strong>: Po vklopu mora miniti premor pred naslednjim (glej "Minimalni premor med vklopi v utilityju in WC-ju" – če se ventilator vklaplja prepogosto, ga povečajte).</li>
                    <li><strong>Primer</strong>: Nekdo gre na WC, ugasne luč – ventilator prezrači nekaj minut. Če je ponoči glasno, izklopite "Dovoljenje za polsamodejno prezračevanje ponoči" v nastavitvah.</li>
                </ul>

                <h3 id="3-3-kopalnica-ventilator">3.3 Kopalnica ventilator</h3>
                <p><strong>Glavna logika:</strong> Avtomatski po vlagi + polavtomatski + ročni.</p>
                <ul>
                    <li><strong>Vklop (ročni)</strong>: Kratek pritisk tipke (normalni čas), dolg pritisk (2x daljši). Ali preko naprave v dnevni sobi.</li>
                    <li><strong>Vklop (polavtomatski)</strong>: Ko ugasnete luč.</li>
                    <li><strong>Vklop (avtomatski)</strong>: Ko vlaga preseže mejo (glej "Mejna vrednost vlage za kopalnico in utility" – če se ventilator vklopi prepozno, jo znižajte).</li>
                    <li><strong>Izklop</strong>: Po času ("Čas delovanja ventilatorjev po sprožilcu"), ali če je zunanja vlaga ekstremna.</li>
                    <li><strong>Posebno</strong>: Premor med cikli ("Minimalni premor med vklopi v kopalnici" – ločeno, če želite pogostejše prezračevanje).</li>
                    <li><strong>Primer</strong>: Po prhanju vlaga naraste – avtomatski vklop za nekaj minut. Če se kopalnica ne posuši dovolj, povečajte "Čas delovanja ventilatorjev po sprožilcu".</li>
                </ul>

                <h3 id="3-4-utility-ventilator">3.4 Utility ventilator</h3>
                <p><strong>Glavna logika:</strong> Podobno kot kopalnica.</p>
                <ul>
                    <li><strong>Vklop (ročni)</strong>: Stikalo na steni.</li>
                    <li><strong>Vklop (polavtomatski)</strong>: Ugasnjena luč.</li>
                    <li><strong>Vklop (avtomatski)</strong>: Visoka vlaga ("Mejna vrednost vlage za kopalnico in utility").</li>
                    <li><strong>Izklop</strong>: Po času ali ročno.</li>
                    <li><strong>Posebno</strong>: Premor ("Minimalni premor med vklopi v utilityju in WC-ju").</li>
                    <li><strong>Primer</strong>: Sušite perilo, vlaga naraste – avtomatski vklop. Če je premalo, znižajte mejno vlago.</li>
                </ul>

                <h3 id="3-5-dnevni-prostor">3.5 Dnevni prostor - ciklično prezračevanje</h3>
                <p><strong>Glavna logika:</strong> Avtomatski cikli, brez ročnega/polavtomatskega.</p>
                <ul>
                    <li><strong>Cikel</strong>: Obdobje, kjer ventilator dela del časa. Prilagaja se po vlagi/CO2/temperaturi.</li>
                    <li><strong>Povečanje</strong>: Visoka vlaga ("Mejna vrednost vlage za dnevni prostor") ali CO2 ("Nizka meja CO2 za dnevni prostor") – dodatek % ("Nizko povečanje za dnevni prostor"). Če zelo visoko, večji dodatek ("Visoko povečanje za dnevni prostor").</li>
                    <li><strong>Zmanjšanje</strong>: Odprta okna, NND, ekstremna zunanja vlaga/temperatura ("Ekstremno visoka vlaga za dnevni prostor").</li>
                    <li><strong>Stopnje</strong>: 1 (tiho ponoči), 2 (normalno), 3 (močno).</li>
                    <li><strong>Primer</strong>: Več ljudi, CO2 naraste – cikel se poveča. Če se zdi prezračevanje preslabo, povečajte "Aktivni delež cikla za dnevni prostor" ali znižajte "Nizka meja CO2".</li>
                </ul>

                <h3 id="3-6-skupni-vhod">3.6 Skupni vhod</h3>
                <p>Ko katerikoli ventilator dela, se vklopi skupni vhod za svež zrak.</p>

                <h2 id="4-poseben-program-za-susenje">4. Poseben program za sušenje</h2>
                <p>Poseben program za sušenje je namenjen učinkovitemu odstranjevanju vlage v <strong>kopalnici</strong> in <strong>utility prostoru</strong>. Aktivira se samodejno ob velikem porastu vlage (npr. po prhanju ali sušenju perila) ali pa ga zaženete ročno prek spletne strani. Deluje v ciklih: ventilator dela nekaj časa, potem miruje, dokler se vlaga ne zniža na normalno raven.</p>

                <h3 id="4-1-kaj-je-poseben-program">4.1 Kaj je poseben program za sušenje?</h3>
                <ul>
                    <li><strong>Samodejna aktivacija</strong>: Sistem zazna hitro povečanje vlage in začne program.</li>
                    <li><strong>Ročna aktivacija</strong>: Na spletni strani kliknite gumb "Drying" za kopalnico ali utility.</li>
                    <li><strong>Tri stopnje</strong>: Sistem izbere glede na temperaturo in vlago:
                        <ul>
                            <li><strong>Stopnja 1 (lahka)</strong>: Pri nižji temperaturi ali zmerni vlagi.</li>
                            <li><strong>Stopnja 2 (srednja)</strong>: Pri srednji temperaturi ali visoki vlagi.</li>
                            <li><strong>Stopnja 3 (močna)</strong>: Pri visoki temperaturi ali zelo visoki vlagi.</li>
                        </ul>
                    </li>
                    <li><strong>Cikel</strong>: Sestavljen iz 3 obdobij dela ventilatorja in 2 premorov. Trajanje je odvisno od stopnje.</li>
                    <li><strong>Konec</strong>: Ko se vlaga zniža ali po treh ciklih, se program konča.</li>
                </ul>

                <h3 id="4-2-kako-deluje-v-kopalnici">4.2 Kako deluje v kopalnici?</h3>
                <p><strong>Primer</strong>: Po prhanju vlaga naraste. Sistem aktivira program (npr. stopnja 2). Ventilator dela nekaj minut, potem miruje, in tako naprej. Na spletni strani vidite status "Drying" in čas do konca.</p>
                <p><strong>Ročna aktivacija</strong>: Kliknite "Drying" za kopalnico – program se začne takoj.</p>

                <h3 id="4-3-kako-deluje-v-utilityju">4.3 Kako deluje v utilityju?</h3>
                <p>Podobno kot v kopalnici, ampak sistem počaka, da se vlaga stabilizira, da se izogne nepotrebnemu delovanju.</p>
                <p><strong>Primer</strong>: Sušite perilo, vlaga naraste in ostane visoka. Sistem začne program z krajšimi obdobji dela in daljšimi premori.</p>

                <h3 id="4-4-kaj-vidite-na-spletni-strani">4.4 Kaj vidite na spletni strani?</h3>
                <ul>
                    <li><strong>Status</strong>: Namesto "ON" ali "OFF" vidite "Drying 1", "Drying 2" ali "Drying 3".</li>
                    <li><strong>Preostali čas</strong>: Čas do konca programa.</li>
                    <li><strong>Ponoči</strong>: Če je nočni čas in imate izklopljeno samodejno prezračevanje ponoči, se program ne aktivira samodejno. Ročni pa deluje.</li>
                </ul>

                <h3 id="4-5-kako-prilagodite-program">4.5 Kako prilagodite program?</h3>
                <p>Prilagodite parametre v nastavitvah, kot so čas delovanja ventilatorja, premori med vklopi in mejna vlaga za aktivacijo.</p>
                <p><strong>Primer</strong>: Če kopalnica ostane vlažna, povečajte čas delovanja in znižajte mejno vlago v nastavitvah.</p>

                <h3 id="4-6-posebni-primeri">4.6 Posebni primeri</h3>
                <ul>
                    <li><strong>Dodatno sušenje</strong>: Med programom pritisnite tipko – ventilator bo delal dlje.</li>
                    <li><strong>Izklop</strong>: Če izklopite ventilator prek strani, se program prekine.</li>
                    <li><strong>Če senzor ne deluje</strong>: Program se ne more aktivirati.</li>
                </ul>

                <h2 id="5-nocni-in-dnevni-cas">5. Nočni in dnevni čas – omejitve</h2>
                <p>Sistem ima omejitve za nočni čas (ko spite) in čas, ko ni nikogar doma, da zmanjša hrup in porabo energije.</p>

                <h3 id="5-1-nocni-cas">5.1 Nočni čas (običajno 22:00–06:00)</h3>
                <p>V nočnem času so omejene nekatere funkcije prezračevanja, da ne motijo spanja.</p>
                <ul>
                    <li><strong>Dovoljenje za samodejno prezračevanje ponoči</strong>: Ali se ventilatorji vklopijo sami po vlagi ponoči.</li>
                    <li><strong>Dovoljenje za polsamodejno prezračevanje ponoči</strong>: Ali se vklopijo ob ugasnjeni luči ponoči.</li>
                    <li><strong>Dovoljenje za ročno prezračevanje ponoči</strong>: Ali delujejo ročni vklopi ponoči.</li>
                    <li><strong>Primer</strong>: Če vas ventilator budi po nočnem obisku kopalnice, izklopite samodejno prezračevanje ponoči v nastavitvah.</li>
                </ul>

                <h3 id="5-2-ko-ni-nikogar-doma">5.2 Ko ni nikogar doma (delavniki, običajno 06:00–16:00)</h3>
                <p>Ko ni nikogar doma, se dnevni prostor ne prezračuje samodejno, da varčuje energijo.</p>
                <ul>
                    <li>Dnevni prostor miruje avtomatsko.</li>
                    <li>Ostali prostori (WC, kopalnica, utility) delujejo normalno.</li>
                    <li><strong>Primer</strong>: Med delom – manj porabe. Vikendi in prazniki: normalno.</li>
                </ul>

                <h2 id="6-vpliv-zunanjega-vremena">6. Vpliv zunanjega vremena</h2>
                <p>Sistem prilagaja po zunanjih podatkih, da ne vnaša mrzlega/toplotnega zraka.</p>

                <table>
                    <thead>
                        <tr>
                            <th>Zunanja temperatura</th>
                            <th>Učinek</th>
                            <th>Povezani parametri</th>
                        </tr>
                    </thead>
                    <tbody>
                        <tr>
                            <td>Nad 5°C</td>
                            <td>Normalno</td>
                            <td>-</td>
                        </tr>
                        <tr>
                            <td>Med 5°C in -10°C</td>
                            <td>Trajanje ventilatorjev se skrajša na polovico</td>
                            <td>"Prag za zmanjšanje delovanja pri nizki zunanji temperaturi" (privzeto 5°C) – če želite manj prepiha, dvignite na 10°C.</td>
                        </tr>
                        <tr>
                            <td>Pod -10°C</td>
                            <td>Ustavitev ventilatorjev</td>
                            <td>"Prag za ustavitev delovanja pri minimalni zunanji temperaturi" (privzeto -10°C) – če želite delovanje pri -15°C, znižajte.</td>
                        </tr>
                        <tr>
                            <td>Nad 30°C</td>
                            <td>Zmanjšanje cikla v dnevnem prostoru</td>
                            <td>"Ekstremno visoka temperatura za dnevni prostor".</td>
                        </tr>
                    </tbody>
                </table>

                <p><strong>Primer</strong>: Pozimi pod 0°C – krajši cikli. Če želite več prezračevanja, prilagodite meje v nastavitvah (glej razdelek 7.1).</p>

                <h2 id="7-nastavitve">7. Nastavitve - podroben opis</h2>
                <p>Tu so vsi parametri z razumljivimi opisi. Vsak ima: kaj naredi, trenutno vrednost (iz vaše enote), obseg in kako vpliva (s primeri in povezavami na logike). Spreminjajte na "/settings".</p>

                <h3 id="7-1-splosne-nastavitve">7.1 Splošne nastavitve za Kopalnico, Utility, WC</h3>

                <p><strong>Mejna vrednost vlage za kopalnico in utility</strong><br>
                Kaj naredi: Relativna vlaga, pri kateri se ventilator samodejno vklopi (avtomatski način). <strong>Privzeta vrednost: 65 %</strong>.<br>
                Obseg: 0–100 %<br>
                Kako vpliva: Če se ventilator vklopi prepozno po prhanju, znižajte na 50 % – hitrejše odzivanje. Če prepogosto, dvignite na 70 % – manj delovanja. Vpliva na avtomatski način v kopalnici/utility.</p>

                <p><strong>Čas delovanja ventilatorjev po sprožilcu</strong><br>
                Kaj naredi: Čas delovanja ventilatorja po sprožilcu (ročni, polavtomatski ali avtomatski). <strong>Privzeta vrednost: 180 s (3 minute)</strong>.<br>
                Obseg: 60–6000 s<br>
                Kako vpliva: Če kopalnica ostane vlažna po prhanju, povečajte na 300 s – temeljitejše sušenje. Če je preglasno, skrajšajte na 120 s. Vpliva na vse načine v WC/kopalnici/utility.</p>

                <p><strong>Minimalni premor med vklopi v utilityju in WC-ju</strong><br>
                Kaj naredi: Minimalni premor med zaporednimi vklopi ventilatorja v utilityju ali WC-ju. <strong>Privzeta vrednost: 1800 s (30 minut)</strong>.<br>
                Obseg: 60–6000 s<br>
                Kako vpliva: Če se ventilator vklaplja prepogosto (npr. večkrat na uro na WC-ju), povečajte na 2400 s – manj ciklov. Če želite pogostejše, skrajšajte na 600 s. Vpliva na polavtomatski/ročni način.</p>

                <p><strong>Minimalni premor med vklopi v kopalnici</strong><br>
                Kaj naredi: Minimalni premor med zaporednimi vklopi v kopalnici (ločeno). <strong>Privzeta vrednost: 180 s (3 minute)</strong>.<br>
                Obseg: 60–6000 s<br>
                Kako vpliva: Podobno kot zgoraj, ampak samo za kopalnico. Če želite hitrejše ponovno prezračevanje po prhanju, skrajšajte na 60 s.</p>

                <p><strong>Prag za zmanjšanje delovanja pri nizki zunanji temperaturi</strong><br>
                Kaj naredi: Zunanja temperatura, pod katero se trajanje ventilatorjev skrajša na polovico. <strong>Privzeta vrednost: 5 °C</strong>.<br>
                Obseg: -20–40 °C<br>
                Kako vpliva: Če je pozimi prepih, dvignite na 10 °C – krajši cikli že pri 10 °C. Če želite več prezračevanja, znižajte na 0 °C.</p>

                <p><strong>Prag za ustavitev delovanja pri minimalni zunanji temperaturi</strong><br>
                Kaj naredi: Zunanja temperatura, pod katero se ventilatorji ustavijo. <strong>Privzeta vrednost: -10 °C</strong>.<br>
                Obseg: -20–40 °C<br>
                Kako vpliva: Če želite delovanje pri zelo mrazu, znižajte na -15 °C. Če ne želite mrzlega zraka, dvignite na -5 °C.</p>

                <h3 id="7-2-nastavitve-za-dnd">7.2 Nastavitve za DND (nočni čas)</h3>

                <p><strong>Dovoljenje za samodejno prezračevanje ponoči</strong><br>
                Kaj naredi: Dovoli samodejne vklope po vlagi ponoči.<br>
                Kako vpliva: Če vas budi ventilator po nočnem prhanju, izklopite – brez avtomatskih vklopov. Vpliva na avtomatski način.</p>

                <p><strong>Dovoljenje za polsamodejno prezračevanje ponoči</strong><br>
                Kaj naredi: Dovoli vklope ob ugasnjeni luči ponoči.<br>
                Kako vpliva: Če nočni obisk WC-ja povzroča hrup, izklopite. Vpliva na polavtomatski način.</p>

                <p><strong>Dovoljenje za ročno prezračevanje ponoči</strong><br>
                Kaj naredi: Dovoli ročne vklope ponoči.<br>
                Kako vpliva: Če želite popoln mir, izklopite – brez ročnih vklopov ponoči.</p>

                <h3 id="7-3-nastavitve-za-dnevni-prostor">7.3 Nastavitve za Dnevni prostor (ciklično prezračevanje)</h3>

                <p><strong>Trajanje cikla za dnevni prostor</strong><br>
                Kaj naredi: Skupni čas enega cikla. <strong>Privzeta vrednost: 2000 s (33 minut)</strong>.<br>
                Obseg: 60–6000 s<br>
                Kako vpliva: Krajši cikli (npr. 1000 s) = hitrejši odziv na CO2, ampak pogostejši vklopi. Daljši (3000 s) = stabilnejše, manj preklapljanja.</p>

                <p><strong>Aktivni delež cikla za dnevni prostor</strong><br>
                Kaj naredi: Osnovni odstotek časa, ko ventilator dela v ciklu. <strong>Privzeta vrednost: 9 %</strong>.<br>
                Obseg: 0–100 %<br>
                Kako vpliva: Če je zrak slab, povečajte na 15 % – močnejše osnovno prezračevanje. Če je prezračevanje premočno, znižajte na 5 %. Vpliva na avtomatski način.</p>

                <p><strong>Mejna vrednost vlage za dnevni prostor</strong><br>
                Kaj naredi: Vlaga, pri kateri se cikel poveča. <strong>Privzeta vrednost: 60 %</strong>.<br>
                Obseg: 0–100 %<br>
                Kako vpliva: Če vlaga naraste hitro (npr. kuhanje), znižajte na 50 % – hitrejše povečanje.</p>

                <p><strong>Visoka meja vlage za dnevni prostor</strong><br>
                Kaj naredi: Višja vlaga za močnejše povečanje. <strong>Privzeta vrednost: 70 %</strong>.<br>
                Obseg: 0–100 %<br>
                Kako vpliva: Za "izredne" situacije – če želite agresivnejše, znižajte na 65 %.</p>

                <p><strong>Ekstremno visoka vlaga za dnevni prostor</strong><br>
                Kaj naredi: Zunanja vlaga, pri kateri se cikel zmanjša za 50%. <strong>Privzeta vrednost: 85 %</strong>.<br>
                Obseg: 0–100 %<br>
                Kako vpliva: Če zunaj vlage ne želite vnašati, dvignite na 90 % – manj zmanjšanja. Če želite hitro zmanjšanje pri vlažnem vremenu, znižajte na 75 %.</p>

                <p><strong>Nizka meja CO2 za dnevni prostor</strong><br>
                Kaj naredi: CO2 raven za začetno povečanje cikla. <strong>Privzeta vrednost: 900 ppm</strong>.<br>
                Obseg: 400–2000 ppm<br>
                Kako vpliva: Če je zrak "težek" hitro, znižajte na 800 ppm – občutljivejše na ljudi/kuhanje. Če želite manj pogosto povečanje, dvignite na 1000 ppm.</p>

                <p><strong>Visoka meja CO2 za dnevni prostor</strong><br>
                Kaj naredi: Višja CO2 za maksimalno povečanje. <strong>Privzeta vrednost: 1200 ppm</strong>.<br>
                Obseg: 400–2000 ppm<br>
                Kako vpliva: Za zelo slabo kakovost – znižajte, če želite močnejši odziv prej.</p>

                <p><strong>Nizko povečanje za dnevni prostor</strong><br>
                Kaj naredi: Dodatni odstotek cikla ob nizki meji vlage/CO2. <strong>Privzeta vrednost: 5 %</strong>.<br>
                Obseg: 0–100 %<br>
                Kako vpliva: Če se cikel poveča premalo, dvignite na 10 % – močnejši začetni odziv.</p>

                <p><strong>Visoko povečanje za dnevni prostor</strong><br>
                Kaj naredi: Dodatni odstotek ob visoki meji. <strong>Privzeta vrednost: 20 %</strong>.<br>
                Obseg: 0–100 %<br>
                Kako vpliva: Za ekstremne situacije – povečajte, če želite maksimalno prezračevanje hitro.</p>

                <p><strong>Povečanje za temperaturo za dnevni prostor</strong><br>
                Kaj naredi: Dodatni odstotek za uravnavanje temperature (hlajenje/ogrevanje). <strong>Privzeta vrednost: 10 %</strong>.<br>
                Obseg: 0–100 %<br>
                Kako vpliva: Če poleti želite več hlajenja, povečajte na 20 % – uporabi zunanji zrak.</p>

                <p><strong>Idealna temperatura za dnevni prostor</strong><br>
                Kaj naredi: Ciljna temperatura za prilagajanje cikla. <strong>Privzeta vrednost: 23 °C</strong>.<br>
                Obseg: -20–40 °C<br>
                Kako vpliva: Če želite hladnejši dom, znižajte na 22 °C – sistem bo povečal prezračevanje, ko je zunaj hladneje. Če želite toplejše, dvignite na 24 °C.</p>

                <p><strong>Ekstremno visoka temperatura za dnevni prostor</strong><br>
                Kaj naredi: Zunanja temperatura za zmanjšanje cikla. <strong>Privzeta vrednost: 30 °C</strong>.<br>
                Obseg: -20–40 °C<br>
                Kako vpliva: Če ne želite vročega zraka, znižajte na 28 °C – hitrejše zmanjšanje poleti.</p>

                <p><strong>Ekstremno nizka temperatura za dnevni prostor</strong><br>
                Kaj naredi: Zunanja temperatura za zmanjšanje cikla. <strong>Privzeta vrednost: -7 °C</strong>.<br>
                Obseg: -20–40 °C<br>
                Kako vpliva: Podobno, za zimo – dvignite na -5 °C, če želite manj mrzlega zraka.</p>

                <h3 id="7-4-stopnje-ventilatorjev">7.4 Stopnje ventilatorjev v dnevnem prostoru</h3>
                <p>Ventilator v dnevnem prostoru ima tri stopnje, ki določajo jakost prezračevanja:</p>
                <ul>
                    <li><strong>Stopnja 1 (tiho)</strong>: Uporablja se ponoči (DND) ali pri normalnih pogojih. Ventilator dela tiho.</li>
                    <li><strong>Stopnja 2 (normalno)</strong>: Uporablja se pri visoki vlagi (>70%) ali visokem CO2 (>1200 ppm). Ventilator dela z večjo močjo.</li>
                    <li><strong>Stopnja 3 (močno)</strong>: Samo ročni način. Aktivira se, ko ročno vklopite ventilator preko spletne strani. Najmočnejše prezračevanje.</li>
                </ul>
                <p>Sistem samodejno izbere stopnjo glede na pogoje. Ponoči bo vedno stopnja 1 (tiho), da ne moti spanja.</p>

                <h3 id="7-5-natancnejsa-razlaga-nnd">7.5 Natančnejša razlaga NND (ko ni nikogar doma)</h3>
                <p>NND deluje <strong>ob delavnikih od 6:00 do 16:00</strong>. V tem času:</p>
                <ul>
                    <li><strong>Dnevni prostor</strong>: Avtomatsko ciklično prezračevanje je <strong>izklopljeno</strong>. To pomeni, da se ventilator v dnevnem prostoru ne vklopi samodejno, ne glede na CO2 ali vlago.</li>
                    <li><strong>Ostali prostori</strong> (WC, kopalnica, utility) delujejo normalno (ročno, polavtomatsko, avtomatsko).</li>
                    <li><strong>Vikendi in prazniki</strong>: NND <strong>ne</strong> velja, dnevni prostor se normalno prezračuje.</li>
                </ul>
                <p><strong>Namen</strong>: Varčevanje z energijo, ko ni nikogar doma. Če ste pogosto doma ob delavnikih, lahko NND izklopite tako, da nastavite "Dovoljenje za samodejno prezračevanje ponoči" in ostale nastavitve za DND.</p>

                <h3 id="7-6-primeri-prilagajanja">7.6 Primeri prilagajanja</h3>
                <ul>
                    <li><strong>Prekratko prezračevanje po prhanju</strong>: Povečajte čas delovanja ventilatorjev po sprožilcu in znižajte mejno vrednost vlage za kopalnico in utility v nastavitvah.</li>
                    <li><strong>Preveč hrupa ponoči</strong>: Izklopite dovoljenje za samodejno in polsamodejno prezračevanje ponoči v nastavitvah.</li>
                    <li><strong>Slab zrak v dnevnem prostoru</strong>: Znižajte nizko mejo CO2 za dnevni prostor in povečajte nizko povečanje za dnevni prostor v nastavitvah.</li>
                    <li><strong>Prepih pozimi</strong>: Dvignite prag za zmanjšanje delovanja pri nizki zunanji temperaturi v nastavitvah.</li>
                    <li><strong>Premalo prezračevanja poleti</strong>: Povečajte aktivni delež cikla za dnevni prostor v nastavitvah.</li>
                    <li><strong>Kopalnica ostane vlažna po prhanju</strong>: Aktivirajte ročni program za sušenje prek spletne strani ali povečajte čas delovanja ventilatorjev po sprožilcu v nastavitvah.</li>
                </ul>

                <h3 id="7-7-kako-shraniti-nastavitve">7.7 Kako shraniti nastavitve</h3>
                <ol>
                    <li>Pojdite na "Nastavitve".</li>
                    <li>Spremenite vrednosti.</li>
                    <li>Kliknite "Shrani" – sistem potrdi "Nastavitve shranjene!".</li>
                </ol>
                <p><strong>Pomembno</strong>: Po shranjevanju se nastavitve takoj uporabijo. Če naredite napako, lahko ponovno naložite stran in popravite vrednosti.</p>

                <hr>

                <p><em>Dokumentacija verzija 3.0 - Marec 2026</em></p>
            </div>
        </div>
    </div>

    <script>
        // Search functionality with navigation
        const searchInput = document.getElementById('searchInput');
        const searchButton = document.getElementById('searchButton');
        const prevButton = document.getElementById('prevButton');
        const nextButton = document.getElementById('nextButton');
        const searchResults = document.getElementById('searchResults');
        let currentMatches = [];
        let currentMatchIndex = -1;

        function clearHighlights() {
            document.querySelectorAll('.highlight').forEach(el => {
                const parent = el.parentNode;
                parent.replaceChild(document.createTextNode(el.textContent), el);
                parent.normalize();
            });
        }

        function highlightText(text) {
            if (!text) return;
            clearHighlights();
            currentMatches = [];
            currentMatchIndex = -1;

            const elements = document.querySelectorAll('.help-container p, .help-container li, .help-container h2, .help-container h3, .help-container h4, .help-container td, .help-container th');
            const regex = new RegExp('(' + text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + ')', 'gi');

            elements.forEach(el => {
                const originalText = el.textContent;
                if (regex.test(originalText)) {
                    const html = originalText.replace(regex, '<span class="highlight">$1</span>');
                    el.innerHTML = html;
                    el.querySelectorAll('.highlight').forEach(h => currentMatches.push(h));
                }
            });

            if (currentMatches.length > 0) {
                currentMatchIndex = 0;
                updateCurrentMatch();
                searchResults.textContent = `${currentMatches.length} ${currentMatches.length === 1 ? 'zadetek' : 'zadetkov'}`;
                prevButton.disabled = false;
                nextButton.disabled = false;
            } else {
                searchResults.textContent = 'Ni zadetkov';
                prevButton.disabled = true;
                nextButton.disabled = true;
            }
        }

        function updateCurrentMatch() {
            document.querySelectorAll('.highlight.current').forEach(el => el.classList.remove('current'));
            if (currentMatches.length > 0 && currentMatchIndex >= 0) {
                const match = currentMatches[currentMatchIndex];
                match.classList.add('current');
                match.scrollIntoView({ behavior: 'smooth', block: 'center' });
                searchResults.textContent = `Zadetek ${currentMatchIndex + 1}/${currentMatches.length}`;
            }
        }

        function performSearch() {
            const term = searchInput.value.trim();
            if (!term) {
                clearHighlights();
                searchResults.textContent = '';
                prevButton.disabled = true;
                nextButton.disabled = true;
                return;
            }
            highlightText(term);
        }

        searchButton.addEventListener('click', performSearch);
        searchInput.addEventListener('keydown', function(e) {
            if (e.key === 'Enter') performSearch();
        });

        prevButton.addEventListener('click', function() {
            if (currentMatches.length > 0) {
                currentMatchIndex = (currentMatchIndex - 1 + currentMatches.length) % currentMatches.length;
                updateCurrentMatch();
            }
        });

        nextButton.addEventListener('click', function() {
            if (currentMatches.length > 0) {
                currentMatchIndex = (currentMatchIndex + 1) % currentMatches.length;
                updateCurrentMatch();
            }
        });

        // TOC active link on scroll
        const tocLinks = document.querySelectorAll('.toc a');
        const sections = document.querySelectorAll('.help-container h2, .help-container h3');

        function updateActiveTocLink() {
            let current = '';
            sections.forEach(section => {
                const sectionTop = section.offsetTop;
                const scrollPos = window.scrollY + 100;
                if (scrollPos >= sectionTop) {
                    current = section.getAttribute('id');
                }
            });

            tocLinks.forEach(link => {
                link.classList.remove('active');
                if (link.getAttribute('href') === '#' + current) {
                    link.classList.add('active');
                }
            });
        }

        window.addEventListener('scroll', updateActiveTocLink);

        // Smooth scroll for TOC links
        tocLinks.forEach(link => {
            link.addEventListener('click', function(e) {
                e.preventDefault();
                const targetId = this.getAttribute('href').substring(1);
                const targetElement = document.getElementById(targetId);
                if (targetElement) {
                    window.scrollTo({
                        top: targetElement.offsetTop - 80,
                        behavior: 'smooth'
                    });
                }
            });
        });
    </script>
</body>
</html>)rawliteral";

#endif