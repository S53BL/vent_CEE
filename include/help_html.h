#ifndef HELP_HTML_H
#define HELP_HTML_H

const char* HTML_HELP = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>Pomoč - Ventilacijski sistem</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
            font-size: 16px;
            background: #101010;
            color: #e0e0e0;
            display: flex;
        }
        .sidebar {
            position: fixed;
            top: 0;
            left: 0;
            width: 160px;
            height: 100vh;
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 0;
            padding: 20px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.3);
            overflow-y: auto;
            z-index: 1000;
        }
        .sidebar h2 {
            color: #4da6ff;
            margin-top: 0;
            margin-bottom: 20px;
            font-size: 18px;
            text-align: center;
            border-bottom: 2px solid #4da6ff;
            padding-bottom: 5px;
        }
        .sidebar .nav-button {
            display: block;
            width: 100%;
            padding: 15px;
            margin-bottom: 10px;
            background-color: #4da6ff;
            color: #101010;
            text-decoration: none;
            border-radius: 6px;
            font-size: 14px;
            font-weight: bold;
            text-align: center;
            border: none;
            cursor: pointer;
            box-sizing: border-box;
        }
        .sidebar .nav-button:hover {
            background-color: #6bb3ff;
        }
        .main-content {
            margin-left: 180px;
            margin-right: 200px;
            padding: 20px;
        }
        .right-sidebar {
            position: fixed;
            top: 0;
            right: 0;
            width: 180px;
            height: 100vh;
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 0;
            padding: 20px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.3);
            overflow-y: auto;
            z-index: 1000;
        }
        .right-sidebar h3 {
            color: #4da6ff;
            margin-top: 0;
            margin-bottom: 15px;
            font-size: 16px;
            text-align: center;
            border-bottom: 2px solid #4da6ff;
            padding-bottom: 5px;
        }
        .toc-list {
            list-style: none;
            padding: 0;
            margin: 0;
        }
        .toc-list li {
            margin-bottom: 8px;
        }
        .toc-list a {
            color: #e0e0e0;
            text-decoration: none;
            font-size: 13px;
            display: block;
            padding: 3px 5px;
            border-radius: 3px;
            transition: background-color 0.2s;
        }
        .toc-list a:hover {
            background-color: #333;
            color: #4da6ff;
        }
        .toc-list .toc-sub {
            margin-left: 10px;
            font-size: 12px;
        }
        .search-container {
            margin-bottom: 20px;
            display: flex;
            flex-direction: column;
            gap: 5px;
        }
        .search-input {
            flex: 1;
            padding: 8px;
            background: #2a2a2a;
            border: 1px solid #555;
            border-radius: 4px;
            color: #e0e0e0;
            font-size: 13px;
            box-sizing: border-box;
        }
        .search-input:focus {
            outline: none;
            border-color: #4da6ff;
        }
        .search-button {
            padding: 8px 12px;
            background: #4da6ff;
            color: #101010;
            border: none;
            border-radius: 4px;
            cursor: pointer;
            font-size: 13px;
            font-weight: bold;
        }
        .search-button:hover {
            background: #6bb3ff;
        }
        .search-results {
            margin-top: 10px;
            font-size: 12px;
            color: #aaa;
        }
        .highlight {
            background-color: #ffeb3b;
            color: #000;
            padding: 2px 4px;
            border-radius: 2px;
        }
        h1 {
            text-align: center;
            font-size: 24px;
            color: #e0e0e0;
            margin-bottom: 30px;
        }
        .help-container {
            max-width: 800px;
            margin: 0 auto;
            background: #1a1a1a;
            border: 1px solid #333;
            border-radius: 8px;
            padding: 20px;
        }
        .nav-link {
            display: inline-block;
            margin-bottom: 20px;
            padding: 10px 20px;
            background-color: #4da6ff;
            color: #101010;
            text-decoration: none;
            border-radius: 6px;
            font-weight: bold;
        }
        .nav-link:hover {
            background-color: #6bb3ff;
        }
        h2 {
            color: #4da6ff;
            margin-top: 30px;
            margin-bottom: 10px;
        }
        h3 {
            color: #4da6ff;
            margin-top: 20px;
            margin-bottom: 8px;
        }
        h4 {
            color: #6bb3ff;
            margin-top: 15px;
            margin-bottom: 5px;
        }
        p {
            margin-bottom: 15px;
            line-height: 1.4;
        }
        ul, ol {
            margin-bottom: 20px;
            padding-left: 20px;
        }
        li {
            margin-bottom: 8px;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin-bottom: 20px;
            background: #252525;
            border-radius: 4px;
            overflow: hidden;
        }
        th, td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #333;
        }
        th {
            background-color: #2a2a2a;
            color: #e0e0e0;
            font-weight: bold;
        }
        tr:hover {
            background-color: #2a2a2a;
        }
        strong {
            color: #4da6ff;
        }
        hr {
            border: none;
            border-top: 1px solid #333;
            margin: 30px 0;
        }
        em {
            color: #aaa;
            font-style: italic;
        }
    </style>
</head>
<body>
    <div class="sidebar">
        <h2>Navigacija</h2>
        <a href="/" class="nav-button">Domača stran</a>
        <a href="/settings" class="nav-button">Nastavitve</a>
        <a href="/sensor-offsets" class="nav-button">Nastavitve senzorjev</a>
        <a href="/help" class="nav-button">Pomoč</a>
        <h3 style="color: #4da6ff; margin: 20px 0 10px 0; font-size: 14px;">Druge enote</h3>
        <a href="http://192.168.2.190/" class="nav-button" style="font-size: 12px;">REW</a>
        <a href="http://192.168.2.191/" class="nav-button" style="font-size: 12px;">SEW</a>
        <a href="http://192.168.2.193/" class="nav-button" style="font-size: 12px;">UT_DEW</a>
        <a href="http://192.168.2.194/" class="nav-button" style="font-size: 12px;">KOP_DEW</a>
    </div>
    <div class="right-sidebar">
        <h3>Iskanje po strani</h3>
        <div class="search-container">
            <input type="text" id="searchInput" class="search-input" placeholder="Vnesite iskalni niz...">
            <button type="button" id="searchButton" class="search-button">Išči</button>
            <div id="searchResults" class="search-results"></div>
        </div>

        <h3>Kazalo vsebine</h3>
        <ul class="toc-list">
            <li><a href="#1-uvod-v-sistem">1. Uvod v sistem</a>
                <ul class="toc-list toc-sub">
                    <li><a href="#1-1-kaj-je-ventilacijski-sistem">1.1 Kaj je ventilacijski sistem?</a></li>
                    <li><a href="#1-2-katere-prostore-sistem-nadzira">1.2 Katere prostore sistem nadzira?</a></li>
                </ul>
            </li>
            <li><a href="#2-razumevanje-spletne-strani">2. Razumevanje spletne strani</a>
                <ul class="toc-list toc-sub">
                    <li><a href="#2-1-domaca-stran">2.1 Domača stran</a></li>
                    <li><a href="#2-2-stran-z-nastavitvami">2.2 Stran z nastavitvami</a></li>
                    <li><a href="#2-3-stran-za-kalibracijo-senzorjev">2.3 Stran za kalibracijo senzorjev</a></li>
                </ul>
            </li>
            <li><a href="#3-kako-delujejo-ventilatorji">3. Kako delujejo ventilatorji</a>
                <ul class="toc-list toc-sub">
                    <li><a href="#3-1-trije-nacini-krmiljenja">3.1 Trije načini krmiljenja</a></li>
                    <li><a href="#3-2-wc-ventilator">3.2 WC ventilator</a></li>
                    <li><a href="#3-3-kopalnica-ventilator">3.3 Kopalnica ventilator</a></li>
                    <li><a href="#3-4-utility-ventilator">3.4 Utility ventilator</a></li>
                    <li><a href="#3-5-dnevni-prostor-ciklicno-prezracevanje">3.5 Dnevni prostor - ciklično prezračevanje</a></li>
                    <li><a href="#3-6-skupni-vhod">3.6 Skupni vhod</a></li>
                </ul>
            </li>
            <li><a href="#4-dnd-in-nnd-razlaga-casovnih-omejitev">4. DND in NND - razlaga časovnih omejitev</a>
                <ul class="toc-list toc-sub">
                    <li><a href="#4-1-dnd-do-not-disturb-nocni-cas">4.1 DND (Do Not Disturb) - nočni čas</a></li>
                    <li><a href="#4-2-nnd-ko-ni-nikogar-doma">4.2 NND (ko ni nikogar doma)</a></li>
                </ul>
            </li>
            <li><a href="#5-vpliv-zunanje-temperature-na-delovanje">5. Vpliv zunanje temperature na delovanje</a></li>
            <li><a href="#6-nastavitve-podroben-opis">6. Nastavitve - podroben opis</a>
                <ul class="toc-list toc-sub">
                    <li><a href="#6-1-sploshne-nastavitve-za-kopalnico-utility-wc">6.1 Splošne nastavitve za Kopalnico, Utility, WC</a></li>
                    <li><a href="#6-2-nastavitve-za-dnd-nocni-cas">6.2 Nastavitve za DND (nočni čas)</a></li>
                    <li><a href="#6-3-nastavitve-za-dnevni-prostor-ciklicno-prezracevanje">6.3 Nastavitve za Dnevni prostor (ciklično prezračevanje)</a></li>
                    <li><a href="#6-4-primeri-prilagajanja">6.4 Primeri prilagajanja</a></li>
                    <li><a href="#6-5-kako-shraniti-nastavitve">6.5 Kako shraniti nastavitve</a></li>
                </ul>
            </li>
        </ul>
    </div>

    <div class="main-content">
        <h1>Pomoč - Ventilacijski sistem</h1>

        <div class="help-container">
<p>Pomoč za Ventilacijski sistem - Navodila </p>
<p>Pomoč je namenjena uporabnikom, ki uporabljate ventilacijski sistem. Sistem je avtomatiziran, ampak ga lahko prilagajate prek spletne strani. Če odprete spletno stran enote (npr. na naslovu 192.168.2.192), vidite glavni zaslon z okni za prostore in funkcije. Če se ne znajdete, kliknite povezavo "Pomoč" na vrhu strani – ta vas pripelje sem. </p>
<p>Priročnik razloži osnove: kako deluje prezračevanje v vsakem prostoru, kaj sproži ventilatorje (ročno, polavtomatsko ali avtomatsko) in kako prilagajate nastavitve na strani "/settings". Če se vam zdi prezračevanje prekratko ali preglasno, boste tukaj našli, kateri parameter popraviti. Vse je razloženo s primeri, brez tehničnih podrobnosti. Če ne želite brati vsega, preskočite na razdelek, ki vas zanima, uporabite iskanje na desni strani (npr. "Kako delujejo ventilatorji" za logike delovanja).</p>
<h2 id="1-uvod-v-sistem">1. Uvod v sistem</h2>
<h3 id="1-1-kaj-je-ventilacijski-sistem">1.1 Kaj je ventilacijski sistem ?</h3>
<h3>1.1 Kaj je ventilacijski sistem ?</h3>
<p>Sistem prezračuje vaš dom avtomatizirano: meri vlago, temperaturo in CO2, da ohranja svež zrak brez ročnega poseganja. Deluje na tri načine – ročno (vi vklopite), polavtomatsko (reagira na vaše akcije, npr. ugasnjeno luč) ali avtomatsko (sam zazna visoko vlago). To pomeni manj vlage v kopalnici po prhanju, boljši zrak v dnevnem prostoru med kuhanjem in manj hrupa ponoči.</p>
<p>Spletna stran je glavni način upravljanja: na domači strani vidite trenutne podatke, na "Nastavitve" prilagajate parametre. Če ste novi, začnite z branjem razdelka 2 za opis strani.</p>
<h3>1.2 Katere prostore sistem nadzira?</h3>
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
<hr />
<h2>2. Razumevanje spletne strani</h2>
<p>Sistem ima tri načine krmiljenja za vsak prostor: ročni (vi ukrepate), polavtomatski (reagira na vaše akcije) in avtomatski (sam meri). Vsak prostor ima prilagojeno logiko. Tukaj so primeri, kdaj se ventilator vklopi/izklopi, in kako to prilagajate v nastavitvah.</p>
<h3>2.1 Domača stran</h3>
<p>To je glavni zaslon z okni (karticami) za vsak prostor in sistem. Vsako okno kaže status v realnem času.</p>
<h4>Statusi in barve</h4>
<ul>
<li><strong>ON / VKLOPLJEN</strong>: Ventilator dela.</li>
<li><strong>OFF / IZKOPLJEN</strong> : Ventilator miruje.</li>
<li><strong>DISABLED / ONEMOGOČEN</strong> : Ročno izklopljen (preko spleta ali stikala).</li>
<li><strong>Preostali čas</strong>: Šteje nazaj sekunde do izklopa ventilatorja.</li>
</ul>
<h4>Okna na domači strani:</h4>
<ul>
<li><strong>WC</strong>: Kaže, ali je luč prižgana, ali ventilator dela in preostali čas. </li>
<li><strong>Kopalnica</strong>: Prikazuje temperaturo/vlago, status tipke/luči in ventilatorja. </li>
<li><strong>Utility</strong>: Podobno kot kopalnica, plus status stikala in luči.</li>
<li><strong>Dnevni prostor</strong>: Temperatura, vlaga, CO2; status oken/vrat; stopnja ventilatorja (0-3) in odstotek aktivnosti cikla.</li>
<li><strong>Zunanji podatki</strong>: Zunanja temperatura/vlaga/tlak; ali je DND (noč) ali NND (ni doma).</li>
<li><strong>Napajanje</strong>: Napetosti, poraba energije.</li>
<li><strong>Sistem</strong>: Uporaba RAM-a, uptime, log velikost.</li>
<li><strong>Status</strong>: Ali senzorji delujejo, napake.</li>
</ul>
<p>Na vrhu strani so povezave: "Nastavitve" (za prilagajanje parametrov), "Nastavitve senzorjev" (če odčitki niso točni), "Pomoč" (ta priročnik).</p>
<h3>2.2 Stran z nastavitvami (Nastavitve)</h3>
<p>Tu prilagajate parametre. Vsak parameter ima polje za vrednost, opis in obseg. Spremenite vrednost, kliknite "Shrani" – sprememba velja takoj. Če ne veste, kaj parameter naredi, glej razdelek 6 tega priročnika, kjer so vsi razloženi s primeri.</p>
<h3>2.3 Stran za kalibracijo senzorjev (Nastavitve senzorjev)</h3>
<p>Če senzorji kažejo napačno temperaturo/vlago (npr. +1°C preveč), tu dodate popravek (offset). Vnesite vrednost, shrani – senzorji se popravijo. Uporabite, če primerjate z drugim termometrom.</p>
<hr />
<h2>3. Kako delujejo ventilatorji</h2>
<p>Sistem ima tri načine krmiljenja za vsak prostor: ročni (vi ukrepate), polavtomatski (reagira na vaše akcije) in avtomatski (sam meri). Vsak prostor ima prilagojeno logiko. Tukaj so primeri, kdaj se ventilator vklopi/izklopi, in kako to prilagajate v nastavitvah.</p>
<h3>3.1 Trije načini krmiljenja</h3>
<ul>
<li><strong>Ročni</strong>: Vi pritisnete tipko, stikalo ali gumb. Primer: Pred odhodom pritisnete tipko v kopalnici – ventilator dela določen čas.</li>
<li><strong>Polavtomatski</strong>: Sistem zazna vašo akcijo, npr. ugasnjeno luč. Primer: Ko v WC-ju ugasnete luč – se ventilator vklopi avtomatsko.</li>
<li><strong>Avtomatski</strong>: Sistem meri vlago/CO2 in ukrepa sam. Primer: Po prhanju vlaga naraste – ventilator se vklopi brez vašega posega.</li>
</ul>
<h3>3.2 WC ventilator</h3>
<p><strong>Glavna logika:</strong> Polavtomatski + ročni. Brez avtomatskega po vlagi.</p>
<ul>
<li><strong>Vklop (polavtomatski)</strong>: Ko ugasnete luč (prehod ON → OFF). Ventilator dela določen čas (glej parameter "Čas delovanja ventilatorjev po sprožilcu" v nastavitvah – če se vam zdi prekratek, ga povečajte na 300 s).</li>
<li><strong>Vklop (ročni)</strong>: Preko naprave v dnevni sobi ali stikala.</li>
<li><strong>Izklop</strong>: Po času ali ročno.</li>
<li><strong>Posebno</strong>: Po vklopu mora miniti premor pred naslednjim (glej "Minimalni premor med vklopi v utilityju in WC-ju" – če se ventilator vklaplja prepogosto, povečajte na 1800 s).</li>
<li><strong>Primer</strong>: Nekdo gre na WC, ugasne luč – ventilator prezrači 3 minute. Če je ponoči glasno, izklopite "Dovoljenje za polsamodejno prezračevanje ponoči" v nastavitvah.</li>
</ul>
<h3>3.3 Kopalnica ventilator</h3>
<p><strong>Glavna logika:</strong> Avtomatski po vlagi + polavtomatski + ročni.</p>
<ul>
<li><strong>Vklop (ročni)</strong>: Kratek pritisk tipke (normalni čas), dolg pritisk (2x daljši). Ali preko naprave v dnevni sobi.</li>
<li><strong>Vklop (polavtomatski)</strong>: Ko ugasnete luč.</li>
<li><strong>Vklop (avtomatski)</strong>: Ko vlaga preseže mejo (glej "Mejna vrednost vlage za kopalnico in utility" – če se ventilator vklopi prepozno, znižajte na 50 %).</li>
<li><strong>Izklop</strong>: Po času ("Čas delovanja ventilatorjev po sprožilcu"), ali če je zunanja vlaga ekstremna.</li>
<li><strong>Posebno</strong>: Premor med cikli ("Minimalni premor med vklopi v kopalnici" – ločeno, če želite pogostejše prezračevanje).</li>
<li><strong>Primer</strong>: Po prhanju vlaga 80 % – avtomatski vklop za 3 minute. Če se kopalnica ne posuši dovolj, povečajte "Čas delovanja ventilatorjev po sprožilcu" na 600 s.</li>
</ul>
<h3>3.4 Utility ventilator</h3>
<p><strong>Glavna logika:</strong> Podobno kot kopalnica.</p>
<ul>
<li><strong>Vklop (ročni)</strong>: Stikalo na steni.</li>
<li><strong>Vklop (polavtomatski)</strong>: Ugasnjena luč.</li>
<li><strong>Vklop (avtomatski)</strong>: Visoka vlaga ("Mejna vrednost vlage za kopalnico in utility").</li>
<li><strong>Izklop</strong>: Po času ali ročno.</li>
<li><strong>Posebno</strong>: Premor ("Minimalni premor med vklopi v utilityju in WC-ju").</li>
<li><strong>Primer</strong>: Sušite perilo, vlaga naraste – avtomatski vklop. Če je premalo, znižajte mejo vlage.</li>
</ul>
<h3>3.5 Dnevni prostor - ciklično prezračevanje</h3>
<p><strong>Glavna logika:</strong> Avtomatski cikli, brez ročnega/polavtomatskega.</p>
<ul>
<li><strong>Cikel</strong>: Obdobje, kjer ventilator dela del časa (npr. 30 % od 600 s = 180 s dela). Prilagaja se po vlagi/CO2/temperaturi.</li>
<li><strong>Povečanje</strong>: Visoka vlaga ("Mejna vrednost vlage za dnevni prostor") ali CO2 ("Nizka meja CO2 za dnevni prostor") – dodatek % ("Nizko povečanje za dnevni prostor"). Če zelo visoko, večji dodatek ("Visoko povečanje za dnevni prostor").</li>
<li><strong>Zmanjšanje</strong>: Odprta okna, NND, ekstremna zunanja vlaga/temperatura ("Ekstremno visoka vlaga za dnevni prostor").</li>
<li><strong>Stopnje</strong>: 1 (tiho ponoči), 2 (normalno), 3 (močno).</li>
<li><strong>Primer</strong>: Več ljudi, CO2 1000 ppm – cikel se poveča za 15 %. Če se zdi prezračevanje preslabo, povečajte "Aktivni delež cikla za dnevni prostor" na 40 % ali znižajte "Nizka meja CO2" na 800 ppm.</li>
</ul>
<h3>3.6 Skupni vhod</h3>
<p>Ko katerikoli ventilator dela, se vklopi skupni vhod za svež zrak.</p>
<hr />
<h2>4. DND in NND - razlaga časovnih omejitev</h2>
<p>Te omejitve zmanjšajo hrup/porabo, ko ni potrebe.</p>
<h3>4.1 DND (Do Not Disturb) - nočni čas (22:00–06:00)</h3>
<ul>
<li><strong>Dovoljenje za samodejno prezračevanje ponoči</strong>: Samodejni vklopi po vlagi ponoči. Če moti, izklopite – ventilatorji se ne vklopijo sami.</li>
<li><strong>Dovoljenje za polsamodejno prezračevanje ponoči</strong>: Vklopi ob ugasnjeni luči ponoči. Primer: Nočni WC – če želite mir, izklopite.</li>
<li><strong>Dovoljenje za ročno prezračevanje ponoči</strong>: Ročni vklopi ponoči. Priporočeno vključeno.</li>
<li><strong>Primer</strong>: Če vas budi ventilator po nočnem prhanju, izklopite "Dovoljenje za samodejno prezračevanje ponoči" v nastavitvah.</li>
</ul>
<h3>4.2 NND (ko ni nikogar doma) - delavniki (06:00–16:00)</h3>
<ul>
<li>Dnevni prostor se ne prezračuje avtomatsko.</li>
<li>Ostali prostori delujejo normalno.</li>
<li><strong>Primer</strong>: Med delom ni doma – varčuje energijo. Vikendi: normalno delovanje.</li>
</ul>
<hr />
<h2>5. Vpliv zunanje temperature na delovanje</h2>
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
<td>Nad 6°C</td>
<td>Normalno</td>
<td>-</td>
</tr>
<tr>
<td>Med 6°C in -11°C</td>
<td>Trajanje na polovico</td>
<td>"Prag za zmanjšanje delovanja pri nizki zunanji temperaturi" – če želite manj prepiha, dvignite na 10°C.</td>
</tr>
<tr>
<td>Pod -11°C</td>
<td>Ustavitev</td>
<td>"Prag za ustavitev delovanja pri minimalni zunanji temperaturi" – če želite delovanje pri -15°C, znižajte.</td>
</tr>
<tr>
<td>Nad 30°C</td>
<td>Zmanjšanje</td>
<td>"Ekstremno visoka temperatura za dnevni prostor".</td>
</tr>
</tbody>
</table>
<p><strong>Primer</strong>: Pozimi pod 0°C – krajši cikli. Če želite več prezračevanja, prilagodite meje v nastavitvah.</p>
<hr />
<h2>6. Nastavitve - podroben opis</h2>
<p>Tu so vsi parametri z razumljivimi opisi. Vsak ima: kaj naredi, trenutno vrednost (iz vaše enote), obseg in kako vpliva (s primeri in povezavami na logike). Spreminjajte na "/settings".</p>
<h3>6.1 Splošne nastavitve za Kopalnico, Utility, WC</h3>
<p><strong>Mejna vrednost vlage za kopalnico in utility</strong><br />
Kaj naredi: Relativna vlaga, pri kateri se ventilator samodejno vklopi (avtomatski način).<br />
Obseg: 0–100 %<br />
Kako vpliva: Če se ventilator vklopi prepozno po prhanju, znižajte na 50 % – hitrejše odzivanje. Če prepogosto, dvignite na 60 % – manj delovanja. Vpliva na avtomatski način v kopalnici/utility.</p>
<p><strong>Čas delovanja ventilatorjev po sprožilcu</strong><br />
Kaj naredi: Čas delovanja ventilatorja po sprožilcu (ročni, polavtomatski ali avtomatski).<br />
Obseg: 60–6000 s<br />
Kako vpliva: Če kopalnica ostane vlažna po prhanju, povečajte na 300 s – temeljitejše sušenje. Če je preglasno, skrajšajte na 120 s. Vpliva na vse načine v WC/kopalnici/utility.</p>
<p><strong>Minimalni premor med vklopi v utilityju in WC-ju</strong><br />
Kaj naredi: Minimalni premor med zaporednimi vklopi ventilatorja v utilityju ali WC-ju.<br />
Obseg: 60–6000 s<br />
Kako vpliva: Če se ventilator vklaplja prepogosto (npr. večkrat na uro na WC-ju), povečajte na 1800 s – manj ciklov. Če želite pogostejše, skrajšajte na 600 s. Vpliva na polavtomatski/ročni način.</p>
<p><strong>Minimalni premor med vklopi v kopalnici</strong><br />
Kaj naredi: Minimalni premor med zaporednimi vklopi v kopalnici (ločeno).<br />
Obseg: 60–6000 s<br />
Kako vpliva: Podobno kot zgoraj, ampak samo za kopalnico. Če želite hitrejše ponovno prezračevanje po prhanju, skrajšajte.</p>
<p><strong>Prag za zmanjšanje delovanja pri nizki zunanji temperaturi</strong><br />
Kaj naredi: Zunanja temperatura, pod katero se trajanje ventilatorjev skrajša na polovico.<br />
Obseg: -20–40 °C<br />
Kako vpliva: Če je pozimi prepih, dvignite na 10 °C – krajši cikli že pri 10 °C. Če želite več prezračevanja, znižajte.</p>
<p><strong>Prag za ustavitev delovanja pri minimalni zunanji temperaturi</strong><br />
Kaj naredi: Zunanja temperatura, pod katero se ventilatorji ustavijo.<br />
Obseg: -20–40 °C<br />
Kako vpliva: Če želite delovanje pri zelo mrazu, znižajte na -15 °C. Če ne želite mrzlega zraka, dvignite na -5 °C.</p>
<h3>6.2 Nastavitve za DND (nočni čas)</h3>
<p><strong>Dovoljenje za samodejno prezračevanje ponoči</strong><br />
Kaj naredi: Dovoli samodejne vklope po vlagi ponoči.<br />
Kako vpliva: Če vas budi ventilator po nočnem prhanju, izklopite – brez avtomatskih vklopov. Vpliva na avtomatski način.</p>
<p><strong>Dovoljenje za polsamodejno prezračevanje ponoči</strong><br />
Kaj naredi: Dovoli vklope ob ugasnjeni luči ponoči.<br />
Kako vpliva: Če nočni obisk WC-ja povzroča hrup, izklopite. Vpliva na polavtomatski način.</p>
<p><strong>Dovoljenje za ročno prezračevanje ponoči</strong><br />
Kaj naredi: Dovoli ročne vklope ponoči.<br />
Kako vpliva: Če želite popoln mir, izklopite – brez ročnih vklopov ponoči.</p>
<h3>6.3 Nastavitve za Dnevni prostor (ciklično prezračevanje)</h3>
<p><strong>Trajanje cikla za dnevni prostor</strong><br />
Kaj naredi: Skupni čas enega cikla (npr. 600 s).<br />
Obseg: 60–6000 s<br />
Kako vpliva: Krajši cikli (npr. 300 s) = hitrejši odziv na CO2, ampak pogostejši vklopi. Daljši = stabilnejše.</p>
<p><strong>Aktivni delež cikla za dnevni prostor</strong><br />
Kaj naredi: Osnovni odstotek časa, ko ventilator dela v ciklu.<br />
Obseg: 0–100 %<br />
Kako vpliva: Če je zrak slab, povečajte na 20 % – močnejše osnovno prezračevanje. Vpliva na avtomatski način.</p>
<p><strong>Mejna vrednost vlage za dnevni prostor</strong><br />
Kaj naredi: Vlaga, pri kateri se cikel poveča.<br />
Obseg: 0–100 %<br />
Kako vpliva: Če vlaga naraste hitro (npr. kuhanje), znižajte na 50 % – hitrejše povečanje.</p>
<p><strong>Visoka meja vlage za dnevni prostor</strong><br />
Kaj naredi: Višja vlaga za močnejše povečanje.<br />
Obseg: 0–100 %<br />
Kako vpliva: Za "izredne" situacije – če želite agresivnejše, znižajte.</p>
<p><strong>Ekstremno visoka vlaga za dnevni prostor</strong><br />
Kaj naredi: Zunanja vlaga, pri kateri se cikel zmanjša.<br />
Obseg: 0–100 %<br />
Kako vpliva: Če zunaj vlage ne želite vnašati, dvignite na 75 % – hitrejše zmanjšanje.</p>
<p><strong>Nizka meja CO2 za dnevni prostor</strong><br />
Kaj naredi: CO2 raven za začetno povečanje cikla.<br />
Obseg: 400–2000 ppm<br />
Kako vpliva: Če je zrak "težek" hitro, znižajte na 800 ppm – občutljivejše na ljudi/kuhanje.</p>
<p><strong>Visoka meja CO2 za dnevni prostor</strong><br />
Kaj naredi: Višja CO2 za maksimalno povečanje.<br />
Obseg: 400–2000 ppm<br />
Kako vpliva: Za zelo slabo kakovost – znižajte, če želite močnejši odziv prej.</p>
<p><strong>Nizko povečanje za dnevni prostor</strong><br />
Kaj naredi: Dodatni odstotek cikla ob nizki meji vlage/CO2.<br />
Obseg: 0–100 %<br />
Kako vpliva: Če se cikel poveča premalo, dvignite na 20 % – močnejši začetni odziv.</p>
<p><strong>Visoko povečanje za dnevni prostor</strong><br />
Kaj naredi: Dodatni odstotek ob visoki meji.<br />
Obseg: 0–100 %<br />
Kako vpliva: Za ekstremne situacije – povečajte, če želite maksimalno prezračevanje hitro.</p>
<p><strong>Povečanje za temperaturo za dnevni prostor</strong><br />
Kaj naredi: Dodatni odstotek za uravnavanje temperature (hlajenje/ogrevanje).<br />
Obseg: 0–100 %<br />
Kako vpliva: Če poleti želite več hlajenja, povečajte na 30 % – uporabi zunanji zrak.</p>
<p><strong>Idealna temperatura za dnevni prostor</strong><br />
Kaj naredi: Ciljna temperatura za prilagajanje cikla.<br />
Obseg: -20–40 °C<br />
Kako vpliva: Če želite hladnejši dom, znižajte na 22 °C – sistem bo povečal prezračevanje, ko je zunaj hladneje.</p>
<p><strong>Ekstremno visoka temperatura za dnevni prostor</strong><br />
Kaj naredi: Zunanja temperatura za zmanjšanje cikla.<br />
Obseg: -20–40 °C<br />
Kako vpliva: Če ne želite vročega zraka, znižajte na 28 °C – hitrejše zmanjšanje poleti.</p>
<p><strong>Ekstremno nizka temperatura za dnevni prostor</strong><br />
Kaj naredi: Zunanja temperatura za zmanjšanje cikla.<br />
Obseg: -20–40 °C<br />
Kako vpliva: Podobno, za zimo – dvignite, če želite manj mrzlega zraka.</p>
<h3>6.4 Primeri prilagajanja</h3>
<ul>
<li><strong>Prekratko prezračevanje po prhanju</strong>: Povečajte "Čas delovanja ventilatorjev po sprožilcu" na 300 s in znižajte "Mejna vrednost vlage za kopalnico in utility" na 50 %.</li>
<li><strong>Preveč hrupa ponoči</strong>: Izklopite "Dovoljenje za samodejno prezračevanje ponoči" in "Dovoljenje za polsamodejno prezračevanje ponoči".</li>
<li><strong>Slab zrak v dnevnem prostoru</strong>: Znižajte "Nizka meja CO2 za dnevni prostor" na 800 ppm in povečajte "Nizko povečanje za dnevni prostor" na 20 %.</li>
<li><strong>Prepih pozimi</strong>: Dvignite "Prag za zmanjšanje delovanja pri nizki zunanji temperaturi" na 10 °C.</li>
<li><strong>Premalo prezračevanja poleti</strong>: Povečajte "Aktivni delež cikla za dnevni prostor" na 40 %.</li>
</ul>
<h3>6.5 Kako shraniti nastavitve</h3>
<ol>
<li>Pojdite na "Nastavitve".</li>
<li>Spremenite vrednosti.</li>
<li>Kliknite "Shrani" – sistem potrdi "Nastavitve shranjene!".</li>
</ol>
<hr />
<p><em>Dokumentacija verzija 2.2 - Februar 2026</em>  </p>
        </div>
    </div>

    <script>
        // Smooth scrolling for anchor links
        document.querySelectorAll('a[href^="#"]').forEach(anchor => {
            anchor.addEventListener('click', function (e) {
                if (anchor.closest('.toc-list')) {
                    e.preventDefault();
                    const searchTerm = anchor.textContent.trim();
                    searchInput.value = searchTerm;
                    performSearch();
                } else {
                    e.preventDefault();
                    const target = document.querySelector(this.getAttribute('href'));
                    if (target) {
                    window.scrollTo({
                        top: target.offsetTop - 50,
                        behavior: 'smooth'
                    });
                    }
                }
            });
        });

        // Search functionality
        const searchInput = document.getElementById('searchInput');
        const searchResults = document.getElementById('searchResults');

        function performSearch() {
            const searchTerm = searchInput.value.trim().toLowerCase();
            if (!searchTerm) {
                searchResults.textContent = '';
                return;
            }
            const contentElements = document.querySelectorAll('.help-container p, .help-container li, .help-container h2, .help-container h3, .help-container h4, .help-container td, .help-container th');
            for (let element of contentElements) {
                const text = element.textContent.toLowerCase();
                if (text.includes(searchTerm)) {
                    window.scrollTo({
                        top: element.offsetTop - 50,
                        behavior: 'smooth'
                    });
                    searchResults.textContent = 'Najdeno';
                    return;
                }
            }
            searchResults.textContent = 'Ni najdeno';
        }

        // Search button click
        const searchButton = document.getElementById('searchButton');
        searchButton.addEventListener('click', performSearch);
    </script>
</body>
</html>)rawliteral";

#endif