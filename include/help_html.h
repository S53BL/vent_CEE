#ifndef HELP_HTML_H
#define HELP_HTML_H

#include "html.h"

// help_html.h - HTML za stran /help
// Nav bar se vključi iz html.h (HTML_NAV_CSS + HTML_NAV_BAR)
// Vsebina (help-container) nespremenjena

const char* HTML_HELP = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>Pomoč - Ventilacijski sistem</title>
    <style>)rawliteral"
    // HTML_NAV_CSS se ne more direktno vključiti v rawliteral - CSS je dodan inline spodaj
    R"rawliteral(
        *{box-sizing:border-box;margin:0;padding:0}
        body{font-family:Arial,sans-serif;font-size:15px;background:#101010;color:#e0e0e0;}
        nav{background:#1a1a1a;border-bottom:1px solid #333;padding:8px 16px;display:flex;gap:8px;flex-wrap:wrap;align-items:center;box-shadow:0 2px 8px rgba(0,0,0,.4);}
        .nav-home{background:#4caf50;color:#fff;border-radius:5px;padding:7px 11px;font-size:17px;text-decoration:none;line-height:1;display:inline-block;}
        .nav-home:hover{background:#66bb6a;}
        .nav-btn{background:#2a2a2a;color:#4da6ff;border:1px solid #4da6ff;border-radius:5px;padding:7px 14px;font-size:13px;font-weight:bold;text-decoration:none;white-space:nowrap;}
        .nav-btn:hover,.nav-btn.current{background:#4da6ff;color:#101010;}
        .nav-sep{width:1px;background:#444;align-self:stretch;margin:0 4px;}
        .nav-ext{background:#2a2a2a;color:#aaa;border:1px solid #444;border-radius:5px;padding:7px 14px;font-size:13px;font-weight:bold;text-decoration:none;white-space:nowrap;}
        .nav-ext:hover{background:#333;color:#e0e0e0;border-color:#666;}
        .wrap{max-width:900px;margin:0 auto;padding:16px;}
        h1{font-size:22px;color:#e0e0e0;margin:16px 0 20px 0;text-align:center;}
        .help-container{background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:20px;box-shadow:0 2px 8px rgba(0,0,0,.3);}
        h2{color:#4da6ff;margin-top:30px;margin-bottom:10px;}
        h3{color:#4da6ff;margin-top:20px;margin-bottom:8px;}
        h4{color:#6bb3ff;margin-top:15px;margin-bottom:5px;}
        p{margin-bottom:15px;line-height:1.4;}
        ul,ol{margin-bottom:20px;padding-left:20px;}
        li{margin-bottom:8px;}
        table{width:100%;border-collapse:collapse;margin-bottom:20px;background:#252525;border-radius:4px;overflow:hidden;}
        th,td{padding:12px;text-align:left;border-bottom:1px solid #333;}
        th{background-color:#2a2a2a;color:#e0e0e0;font-weight:bold;}
        tr:hover{background-color:#2a2a2a;}
        strong{color:#4da6ff;}
        hr{border:none;border-top:1px solid #333;margin:30px 0;}
        em{color:#aaa;font-style:italic;}
        .search-bar{display:flex;gap:8px;margin-bottom:20px;align-items:center;}
        .search-input{flex:1;padding:8px 10px;background:#2a2a2a;border:1px solid #555;border-radius:4px;color:#e0e0e0;font-size:13px;}
        .search-input:focus{outline:none;border-color:#4da6ff;}
        .search-button{padding:8px 14px;background:#4da6ff;color:#101010;border:none;border-radius:4px;cursor:pointer;font-size:13px;font-weight:bold;}
        .search-button:hover{background:#6bb3ff;}
        .search-results{font-size:12px;color:#aaa;margin-left:4px;}
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

    <div class="wrap">
        <h1>Pomoč - Ventilacijski sistem</h1>

        <div class="search-bar">
            <input type="text" id="searchInput" class="search-input" placeholder="Iskanje po vsebini...">
            <button type="button" id="searchButton" class="search-button">Išči</button>
            <span id="searchResults" class="search-results"></span>
        </div>

        <div class="help-container">
<p>Pomoč za Ventilacijski sistem - Navodila </p>
<p>Pomoč je namenjena uporabnikom, ki uporabljate ventilacijski sistem. Sistem je avtomatiziran, ampak ga lahko prilagajate prek spletne strani. Če odprete spletno stran enote (npr. na naslovu 192.168.2.192), vidite glavni zaslon z okni za prostore in funkcije. Če se ne znajdete, kliknite povezavo "Pomoč" na vrhu strani – ta vas pripelje sem. </p>
<p>Priročnik razloži osnove: kako deluje prezračevanje v vsakem prostoru, kaj sproži ventilatorje (ročno, polavtomatsko ali avtomatsko) in kako prilagajate nastavitve na strani "/settings". Če se vam zdi prezračevanje prekratko ali preglasno, boste tukaj našli, kateri parameter popraviti. Vse je razloženo s primeri, brez tehničnih podrobnosti. Če ne želite brati vsega, preskočite na razdelek, ki vas zanima, uporabite iskanje zgoraj (npr. "Kako delujejo ventilatorji" za logike delovanja).</p>
<h2 id="1-uvod-v-sistem">1. Uvod v sistem</h2>
<h3 id="1-1-kaj-je-ventilacijski-sistem">1.1 Kaj je ventilacijski sistem ?</h3>
<p>Sistem prezračuje vaš dom avtomatizirano: meri vlago, temperaturo in CO2, da ohranja svež zrak brez ročnega poseganja. Deluje na tri načine – ročno (vi vklopite), polavtomatsko (reagira na vaše akcije, npr. ugasnjeno luč) ali avtomatsko (sam zazna visoko vlago). To pomeni manj vlage v kopalnici po prhanju, boljši zrak v dnevnem prostoru med kuhanjem in manj hrupa ponoči.</p>
<p>Spletna stran je glavni način upravljanja: na domači strani vidite trenutne podatke, na "Nastavitve" prilagajate parametre. Če ste novi, začnite z branjem razdelka 2 za opis strani.</p>
<h3 id="1-2-katere-prostore-sistem-nadzira">1.2 Katere prostore sistem nadzira?</h3>
<p>Sistem upravlja štiri cone:</p>
<table>
<thead>
<tr><th>Prostor</th><th>Opis</th><th>Glavna naloga</th></tr>
</thead>
<tbody>
<tr><td><strong>WC</strong></td><td>Stranišče</td><td>Prezračevanje po uporabi (večinoma polavtomatsko)</td></tr>
<tr><td><strong>Kopalnica</strong></td><td>Kopalnica</td><td>Odstranjevanje vlage po prhanju (avtomatsko + ročno)</td></tr>
<tr><td><strong>Utility</strong></td><td>Pralnica/shramba</td><td>Prezračevanje ob vlagi ali delu (avtomatsko + ročno)</td></tr>
<tr><td><strong>Dnevni prostor</strong></td><td>Bivalni prostor</td><td>Stalno ohranjanje zraka (ciklično, glede na CO2/vlago)</td></tr>
</tbody>
</table>
<hr />
<h2>2. Razumevanje spletne strani</h2>
<h3>2.1 Domača stran</h3>
<p>To je glavni zaslon z okni (karticami) za vsak prostor in sistem. Vsako okno kaže status v realnem času.</p>
<h4>Statusi</h4>
<ul>
<li><strong>ON / VKLOPLJEN</strong>: Ventilator dela.</li>
<li><strong>OFF / IZOKPLJEN</strong>: Ventilator miruje.</li>
<li><strong>DISABLED / ONEMOGOČEN</strong>: Ročno izklopljen.</li>
<li><strong>Preostali čas</strong>: Šteje nazaj sekunde do izklopa ventilatorja.</li>
</ul>
<h4>Okna na domači strani:</h4>
<ul>
<li><strong>WC</strong>: Luč, ventilator, preostali čas.</li>
<li><strong>Kopalnica</strong>: Temperatura/vlaga, status tipke/luči in ventilatorja.</li>
<li><strong>Utility</strong>: Podobno kot kopalnica, plus status stikala.</li>
<li><strong>Dnevni prostor</strong>: Temperatura, vlaga, CO2; status oken/vrat; stopnja ventilatorja in % cikla.</li>
<li><strong>Zunanji podatki</strong>: Zunanja temperatura/vlaga/tlak; DND/NND status.</li>
<li><strong>Napajanje</strong>: Napetosti, poraba energije.</li>
<li><strong>Sistem</strong>: RAM, uptime, log.</li>
<li><strong>Status</strong>: Senzorji, napake.</li>
</ul>
<h3>2.2 Stran z nastavitvami</h3>
<p>Tu prilagajate parametre. Vsak parameter ima polje za vrednost, opis in obseg. Spremenite vrednost, kliknite "Shrani" – sprememba velja takoj.</p>
<h3>2.3 Nastavitve senzorjev</h3>
<p>Če senzorji kažejo napačno temperaturo/vlago, dodate popravek (offset). Vnesite vrednost, shranite – senzorji se popravijo.</p>
<hr />
<h2>3. Kako delujejo ventilatorji</h2>
<h3>3.1 Trije načini krmiljenja</h3>
<ul>
<li><strong>Ročni</strong>: Vi pritisnete tipko, stikalo ali gumb.</li>
<li><strong>Polavtomatski</strong>: Sistem zazna vašo akcijo, npr. ugasnjeno luč.</li>
<li><strong>Avtomatski</strong>: Sistem meri vlago/CO2 in ukrepa sam.</li>
</ul>
<h3>3.2 WC ventilator</h3>
<p><strong>Glavna logika:</strong> Polavtomatski + ročni. Brez avtomatskega po vlagi.</p>
<ul>
<li><strong>Vklop (polavtomatski)</strong>: Ko ugasnete luč (ON → OFF). Čas deluje po parametru "Čas delovanja ventilatorjev po sprožilcu".</li>
<li><strong>Vklop (ročni)</strong>: Preko naprave ali stikala.</li>
<li><strong>Posebno</strong>: Po vklopu mora miniti premor pred naslednjim ("Minimalni premor med vklopi v utilityju in WC-ju").</li>
<li><strong>Primer</strong>: Nekdo gre na WC, ugasne luč – ventilator prezrači 3 minute.</li>
</ul>
<h3>3.3 Kopalnica ventilator</h3>
<p><strong>Glavna logika:</strong> Avtomatski po vlagi + polavtomatski + ročni.</p>
<ul>
<li><strong>Vklop (ročni)</strong>: Kratek pritisk tipke (normalni čas), dolg pritisk (2x daljši).</li>
<li><strong>Vklop (polavtomatski)</strong>: Ko ugasnete luč.</li>
<li><strong>Vklop (avtomatski)</strong>: Ko vlaga preseže mejo ("Mejna vrednost vlage za kopalnico in utility").</li>
<li><strong>Primer</strong>: Po prhanju vlaga 80 % – avtomatski vklop za 3 minute.</li>
</ul>
<h3>3.4 Utility ventilator</h3>
<p><strong>Glavna logika:</strong> Podobno kot kopalnica.</p>
<ul>
<li><strong>Vklop (ročni)</strong>: Stikalo na steni.</li>
<li><strong>Vklop (polavtomatski)</strong>: Ugasnjena luč.</li>
<li><strong>Vklop (avtomatski)</strong>: Visoka vlaga.</li>
<li><strong>Primer</strong>: Sušite perilo, vlaga naraste – avtomatski vklop.</li>
</ul>
<h3>3.5 Dnevni prostor - ciklično prezračevanje</h3>
<p><strong>Glavna logika:</strong> Avtomatski cikli.</p>
<ul>
<li><strong>Cikel</strong>: Obdobje, kjer ventilator dela del časa (npr. 30 % od 600 s = 180 s dela).</li>
<li><strong>Povečanje</strong>: Visoka vlaga ali CO2 – dodatek %.</li>
<li><strong>Zmanjšanje</strong>: Odprta okna, NND, ekstremna zunanja vlaga/temperatura.</li>
<li><strong>Primer</strong>: Več ljudi, CO2 1000 ppm – cikel se poveča za 15 %.</li>
</ul>
<h3>3.6 Skupni vhod</h3>
<p>Ko katerikoli ventilator dela, se vklopi skupni vhod za svež zrak.</p>
<hr />
<h2>4. DND in NND - razlaga časovnih omejitev</h2>
<h3>4.1 DND (Do Not Disturb) - nočni čas (22:00–06:00)</h3>
<ul>
<li><strong>Dovoljenje za samodejno prezračevanje ponoči</strong>: Samodejni vklopi po vlagi ponoči.</li>
<li><strong>Dovoljenje za polsamodejno prezračevanje ponoči</strong>: Vklopi ob ugasnjeni luči ponoči.</li>
<li><strong>Dovoljenje za ročno prezračevanje ponoči</strong>: Ročni vklopi ponoči.</li>
<li><strong>Primer</strong>: Če vas budi ventilator, izklopite "Dovoljenje za samodejno prezračevanje ponoči".</li>
</ul>
<h3>4.2 NND (ko ni nikogar doma) - delavniki (06:00–16:00)</h3>
<ul>
<li>Dnevni prostor se ne prezračuje avtomatsko.</li>
<li>Ostali prostori delujejo normalno.</li>
</ul>
<hr />
<h2>5. Vpliv zunanje temperature na delovanje</h2>
<table>
<thead>
<tr><th>Zunanja temperatura</th><th>Učinek</th><th>Povezani parametri</th></tr>
</thead>
<tbody>
<tr><td>Nad 6°C</td><td>Normalno</td><td>-</td></tr>
<tr><td>Med 6°C in -11°C</td><td>Trajanje na polovico</td><td>"Prag za zmanjšanje delovanja pri nizki zunanji temperaturi"</td></tr>
<tr><td>Pod -11°C</td><td>Ustavitev</td><td>"Prag za ustavitev delovanja pri minimalni zunanji temperaturi"</td></tr>
<tr><td>Nad 30°C</td><td>Zmanjšanje DS cikla</td><td>"Ekstremno visoka temperatura za dnevni prostor"</td></tr>
</tbody>
</table>
<hr />
<h2>6. Nastavitve - podroben opis</h2>
<h3>6.1 Splošne nastavitve za Kopalnico, Utility, WC</h3>
<p><strong>Mejna vrednost vlage za kopalnico in utility</strong><br />Obseg: 0–100 %<br />Če se ventilator vklopi prepozno, znižajte na 50 %. Če prepogosto, dvignite na 60 %.</p>
<p><strong>Čas delovanja ventilatorjev po sprožilcu</strong><br />Obseg: 60–6000 s<br />Če kopalnica ostane vlažna, povečajte na 300 s. Če je preglasno, skrajšajte na 120 s.</p>
<p><strong>Minimalni premor med vklopi v utilityju in WC-ju</strong><br />Obseg: 60–6000 s<br />Če se ventilator vklaplja prepogosto, povečajte na 1800 s.</p>
<p><strong>Minimalni premor med vklopi v kopalnici</strong><br />Obseg: 60–6000 s<br />Ločeno za kopalnico. Skrajšajte za pogostejše prezračevanje.</p>
<p><strong>Prag za zmanjšanje delovanja pri nizki zunanji temperaturi</strong><br />Obseg: -20–40 °C<br />Če je pozimi prepih, dvignite na 10 °C.</p>
<p><strong>Prag za ustavitev delovanja pri minimalni zunanji temperaturi</strong><br />Obseg: -20–40 °C<br />Znižajte na -15 °C za delovanje pri mrazu.</p>
<h3>6.2 Nastavitve za DND (nočni čas)</h3>
<p><strong>Dovoljenje za samodejno prezračevanje ponoči</strong><br />Izklopite, če vas budi ventilator po nočnem prhanju.</p>
<p><strong>Dovoljenje za polsamodejno prezračevanje ponoči</strong><br />Izklopite, če nočni WC povzroča hrup.</p>
<p><strong>Dovoljenje za ročno prezračevanje ponoči</strong><br />Izklopite za popoln mir ponoči.</p>
<h3>6.3 Nastavitve za Dnevni prostor</h3>
<p><strong>Trajanje cikla</strong><br />Obseg: 60–6000 s<br />Krajši = hitrejši odziv, pogostejši vklopi.</p>
<p><strong>Aktivni delež cikla</strong><br />Obseg: 0–100 %<br />Povečajte na 20 % za močnejše osnovno prezračevanje.</p>
<p><strong>Mejna vrednost vlage</strong><br />Obseg: 0–100 %<br />Znižajte na 50 % za hitrejši odziv pri kuhanju.</p>
<p><strong>Visoka meja vlage</strong><br />Obseg: 0–100 %<br />Za močnejše povečanje pri visokih vlaganjih.</p>
<p><strong>Ekstremno visoka zunanja vlaga</strong><br />Obseg: 0–100 %<br />Pri tej vlagi se cikel zmanjša.</p>
<p><strong>Nizka meja CO2</strong><br />Obseg: 400–2000 ppm<br />Znižajte na 800 ppm za hitrejši odziv.</p>
<p><strong>Visoka meja CO2</strong><br />Obseg: 400–2000 ppm<br />Za maksimalno povečanje pri slabem zraku.</p>
<p><strong>Nizko povečanje cikla</strong><br />Obseg: 0–100 %<br />Dvignite na 20 % za močnejši odziv.</p>
<p><strong>Visoko povečanje cikla</strong><br />Obseg: 0–100 %<br />Za ekstremne situacije.</p>
<p><strong>Povečanje za temperaturo</strong><br />Obseg: 0–100 %<br />Povečajte za hlajenje poleti z zunanjim zrakom.</p>
<p><strong>Idealna temperatura</strong><br />Obseg: -20–40 °C<br />Znižajte na 22 °C za hladnejši dom.</p>
<p><strong>Ekstremno visoka zunanja temperatura</strong><br />Obseg: -20–40 °C<br />Znižajte na 28 °C za manj vročega zraka poleti.</p>
<p><strong>Ekstremno nizka zunanja temperatura</strong><br />Obseg: -20–40 °C<br />Dvignite za manj mrzlega zraka pozimi.</p>
<h3>6.4 Primeri prilagajanja</h3>
<ul>
<li><strong>Prekratko prezračevanje po prhanju</strong>: Povečajte "Čas delovanja" na 300 s, znižajte "Mejna vrednost vlage" na 50 %.</li>
<li><strong>Preveč hrupa ponoči</strong>: Izklopite samodejno in polsamodejno ponoči.</li>
<li><strong>Slab zrak v dnevnem prostoru</strong>: Znižajte "Nizka meja CO2" na 800 ppm, povečajte "Nizko povečanje" na 20 %.</li>
<li><strong>Prepih pozimi</strong>: Dvignite "Prag za zmanjšanje" na 10 °C.</li>
<li><strong>Premalo prezračevanja poleti</strong>: Povečajte "Aktivni delež cikla" na 40 %.</li>
</ul>
<h3>6.5 Kako shraniti nastavitve</h3>
<ol>
<li>Pojdite na "Nastavitve".</li>
<li>Spremenite vrednosti.</li>
<li>Kliknite "Shrani" – sistem potrdi "Nastavitve shranjene!".</li>
</ol>
<hr />
<p><em>Dokumentacija verzija 2.3 - Marec 2026</em></p>
        </div>
    </div>

    <script>
        const searchInput = document.getElementById('searchInput');
        const searchResults = document.getElementById('searchResults');

        function performSearch() {
            const term = searchInput.value.trim().toLowerCase();
            if (!term) { searchResults.textContent = ''; return; }
            const els = document.querySelectorAll('.help-container p, .help-container li, .help-container h2, .help-container h3, .help-container h4, .help-container td, .help-container th');
            for (let el of els) {
                if (el.textContent.toLowerCase().includes(term)) {
                    window.scrollTo({ top: el.offsetTop - 60, behavior: 'smooth' });
                    searchResults.textContent = 'Najdeno';
                    return;
                }
            }
            searchResults.textContent = 'Ni najdeno';
        }

        document.getElementById('searchButton').addEventListener('click', performSearch);
        searchInput.addEventListener('keydown', function(e) { if (e.key === 'Enter') performSearch(); });
    </script>
</body>
</html>)rawliteral";

#endif
