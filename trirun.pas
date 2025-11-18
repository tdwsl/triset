program trirun;

uses crt;

var
  r: array[0..15] of word;
  mem: array[0..65535] of word;
  dsk: array[1..67108864] of word;
  dsz: word;
  filename: string;

const
  ascreen = $f000;
  akey = $f000+2000;
  adelay = akey;
  ablock = adelay+1;

procedure switch(a, b: word);
var i: word;
  f: file of word;
begin
  if a = b then exit;
  for i := 0 to 2047 do begin
    dsk[a*2048+i] := mem[$f800+i];
    mem[$f800+i] := dsk[b*2048+i];
  end;
  if a*2048+2048 >= dsz then dsz := a*2048+2048;
  assign(f, filename);
  rewrite(f);
  for i := 0 to dsz-1 do
    write(f, dsk[i]);
  close(f);
end;

procedure st(a, b: word);
var p: word;
begin
  p := mem[a];
  mem[a] := b;
  if (a >= ascreen) and (a < akey) then begin
    if b <= 32 then b := 32;
    a := a-ascreen;
    gotoxy(a mod 80 + 1, a div 80 + 1);
    if (b and $8000) <> 0 then begin
      b := b and $7fff;
      textColor(black);
      textBackground(white);
    end
    else begin
      textColor(white);
      textBackground(black);
    end;
    write(chr(b));
  end
  else if a = adelay then begin
    delay(b);
  end
  else if a = ablock then begin
    switch(p, a);
  end;
end;

function ld(a: word): word;
begin
  ld := mem[a];
  if a = akey then begin
    if keypressed then ld := ord(readkey)
    else ld := 0
  end;
end;

procedure run;
var
  ins: word;
  a, b, c, o: word;
begin
  while true do begin
    o := r[15];
    ins := ld(r[15]); r[15] := r[15]+1;
    a := (ins shr 8) and 15;
    b := r[(ins shr 4) and 15];
    c := ins and 15;
    if (ins and $8000) <> 0 then c := r[c];
    case (ins shr 12) and 7 of
    0: r[a] := b + c;
    1: r[a] := b - c;
    2: r[a] := b and c;
    3: r[a] := not (b or c);
    4: r[a] := (b shr c) or (b shl (16-c));
    5: if b < c then r[a] := 1 else r[a] := 0;
    6: r[a] := ld(b+c);
    7: st(b+c, r[a]);
    end;
    if r[15] = o then break;
  end;
end;

procedure loadFile(name: string);
var
  f: file of word;
  i: word;
begin
  assign(f, name);
  reset(f);
  i := 0;
  while not eof(f) do begin
    read(f, dsk[i]); i := i + 1;
  end;
  dsz := i;
  for i := 0 to 2047 do
    mem[i] := dsk[i];
end;

var i, j: integer;

begin
  if paramCount < 1 then begin
    writeln('no disk supplied');
  end
  else begin
    filename := paramStr(1);
    loadFile(paramStr(1));
    textBackground(lightblue);
    clrscr;
    textBackground(black);
    for i := 1 to 25 do begin
      for j := 1 to 80 do write(' ');
      writeln;
    end;
    cursoroff;
    r[15] := 0;
    run;
    switch(mem[ablock], mem[ablock]+1);
    cursoron;
    readkey;
  end;
end.
