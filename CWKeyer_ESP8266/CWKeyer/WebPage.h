#ifndef WEBPAGE_H
#define WEBPAGE_H

//////////////////////////////////////////////////////
// index_html[]
//////////////////////////////////////////////////////
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>CWKeyer v0.6</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: 'Segoe UI', Arial, sans-serif;
      background: #f4f4f4;
      color: #222;
      margin: 0;
      padding: 0;
    }
    .container {
      max-width: 480px;
      margin: 30px auto;
      background: #fff;
      border-radius: 10px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
      padding: 24px 32px 32px 32px;
    }
    h1 {
      text-align: center;
      color: #005fa3;
      margin-bottom: 10px;
    }
    h2 {
      color: #005fa3;
      border-bottom: 1px solid #eee;
      padding-bottom: 4px;
      margin-top: 28px;
      margin-bottom: 12px;
    }
    form {
      margin-bottom: 18px;
    }
    input[type="text"] {
      width: 70%;
      padding: 6px 8px;
      margin-right: 8px;
      border: 1px solid #bbb;
      border-radius: 4px;
      font-size: 1em;
    }
    input[type="submit"] {
      background: #005fa3;
      color: #fff;
      border: none;
      border-radius: 4px;
      padding: 7px 18px;
      font-size: 1em;
      cursor: pointer;
      transition: background 0.2s;
    }
    input[type="submit"]:hover {
      background: #003e6b;
    }
    fieldset {
      border: 1px solid #bbb;
      border-radius: 6px;
      padding: 10px 14px 10px 14px;
      background: #f9f9f9;
    }
    legend {
      font-weight: bold;
      color: #005fa3;
    }
    label {
      display: inline-block;
      margin: 4px 10px 4px 0;
      font-size: 1.08em;
      cursor: pointer;
    }
    input[type="checkbox"] {
      margin-right: 4px;
      accent-color: #005fa3;
    }
    @media (max-width: 600px) {
      .container {
        padding: 10px 4vw 18px 4vw;
      }
      input[type="text"] {
        width: 60%;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>CWKeyer v0.6</h1>
    <h2>Memory</h2>
    <form action="/get" method="get">
      <label>Mem 1:
        <input type="text" name="input1" maxlength="128" value="%TEXT1%">
      </label>
      <input type="submit" value="Save 1">
    </form>
    <form action="/get" method="get">
      <label>Mem 2:
        <input type="text" name="input2" maxlength="128" value="%TEXT2%">
      </label>
      <input type="submit" value="Save 2">
    </form>
    <h2>Trainer Letters</h2>
    <form action="/save_letters" method="get">
      %LETTERS_CHECKBOXES%
      <br>
      <input type="submit" value="Save">
    </form>
  </div>
</body>
</html>
)rawliteral";


//////////////////////////////////////////////////////
// wrapInPage
//////////////////////////////////////////////////////
String wrapInPage(String content) {
  String html = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>CWKeyer v0.6</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: 'Segoe UI', Arial, sans-serif;
      background: #f4f4f4;
      color: #222;
      margin: 0;
      padding: 0;
    }
    .container {
      max-width: 480px;
      margin: 30px auto;
      background: #fff;
      border-radius: 10px;
      box-shadow: 0 2px 8px rgba(0,0,0,0.08);
      padding: 24px 32px 32px 32px;
      text-align: center;
    }
    h1 {
      text-align: center;
      color: #005fa3;
      margin-bottom: 20px;
    }
    a {
      display: inline-block;
      margin-top: 20px;
      color: #005fa3;
      text-decoration: none;
      font-weight: bold;
    }
    a:hover {
      text-decoration: underline;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>CWKeyer v0.6</h1>
    %CONTENT%
    <br>
    <a href="/">Back</a>
  </div>
</body>
</html>
)rawliteral";

  html.replace("%CONTENT%", content);
  return html;
}

#endif