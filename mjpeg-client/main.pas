{/******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2009 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
#******************************************************************************}

unit main;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, FileUtil, LResources, Forms, Controls, Graphics, Dialogs,
  ExtCtrls, StdCtrls, ComCtrls, lNetComponents, lNet, Spin, Math,
  IniPropStorage;

type

  { TMainForm }

  TMainForm = class(TForm)
    Connect: TButton;
    IniPropStorage1: TIniPropStorage;
    StatusBar: TStatusBar;
    Stop: TButton;
    Host: TEdit;
    Video: TGroupBox;
    VideoFrame: TImage;
    TCPSock: TLTCPComponent;
    Port: TSpinEdit;

    procedure ConnectClick(Sender: TObject);
    procedure StopClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure TCPSockConnect(aSocket: TLSocket);
    procedure TCPSockError(const msg: string; aSocket: TLSocket);
    procedure TCPSockReceive(aSocket: TLSocket);
    procedure DisplayFrame(stream: TStream);
  private

  public
    Stream1: TMemoryStream;
  end; 

var
  MainForm: TMainForm;
  TmpImage : TJPEGImage;
  BmpImage : TBitmap;

implementation

{ TMainForm }

function ByteToHex(InByte:byte):shortstring;
const Digits:array[0..15] of char='0123456789ABCDEF';
begin
 result:=digits[InByte shr 4]+digits[InByte and $0F];
end;

procedure TMainForm.ConnectClick(Sender: TObject);
begin
  Stream1.SetSize(0);
  StatusBar.SimpleText := 'connecting...';
  TCPSock.Connect(Host.Text, Port.Value);
end;

procedure TMainForm.StopClick(Sender: TObject);
begin
  TCPSock.Disconnect;
  StatusBar.SimpleText := 'stopped';
end;

procedure TMainForm.FormCreate(Sender: TObject);
begin
  Stream1 := TMemoryStream.Create;
  TmpImage := TJPEGImage.Create;
  BmpImage := TBitmap.Create;
  Video.DoubleBuffered:=True;

  IniPropStorage1.Restore;
  MainForm.Width := StrToInt(IniPropStorage1.StoredValue['width']);
  MainForm.Height := StrToInt(IniPropStorage1.StoredValue['height']);
  MainForm.Host.Text := IniPropStorage1.StoredValue['host'];
  MainForm.Port.Value := StrToInt(IniPropStorage1.StoredValue['port']);
  MainForm.Left := StrToInt(IniPropStorage1.StoredValue['positionX']);
  MainForm.Top := StrToInt(IniPropStorage1.StoredValue['positionY']);
end;

procedure TMainForm.FormDestroy(Sender: TObject);
begin
  Stream1.Destroy;
  TmpImage.Destroy;
  BmpImage.Destroy;

  IniPropStorage1.StoredValue['width'] := IntToStr(MainForm.Width);
  IniPropStorage1.StoredValue['height'] := IntToStr(MainForm.Height);
  IniPropStorage1.StoredValue['positionX'] := IntToStr(MainForm.Left);
  IniPropStorage1.StoredValue['positionY'] := IntToStr(MainForm.Top);
  IniPropStorage1.StoredValue['host'] := MainForm.Host.Text;
  IniPropStorage1.StoredValue['port'] := IntToStr(MainForm.Port.Value);
  IniPropStorage1.Save;
end;

procedure TMainForm.TCPSockConnect(aSocket: TLSocket);
begin
  StatusBar.SimpleText := 'sending request...';
  aSocket.SendMessage('GET /?action=stream' + #10+ #10);
end;

procedure TMainForm.TCPSockError(const msg: string; aSocket: TLSocket);
begin
  StatusBar.SimpleText := 'connect failed';
  ShowMessage(msg);
end;

procedure TMainForm.DisplayFrame(stream: TStream);
begin
  try
    stream.Position:=0;
    VideoFrame.Picture.Jpeg.LoadFromStream(stream);
  except
    StatusBar.SimpleText := 'error while decoding frame!';
    exit;
  end;
  VideoFrame.Repaint;
  Application.ProcessMessages;
end;

procedure TMainForm.TCPSockReceive(aSocket: TLSocket);
const
  SOI : array[0..1] of Byte = ($FF, $D8);
  EOI : array[0..1] of Byte = ($FF, $D9);
var
  buffer : array[0..65534] of Byte;
  search : array[0..1] of Byte;
  res, i, j : Integer;
  EOIPos, SOIPos, leftover : Int64;
  JPEGStream : TMemoryStream;
  leftoverStream : TMemoryStream;
begin
  res := aSocket.Get( buffer[0], sizeof(buffer) );
  if res = 0 then exit;

  StatusBar.SimpleText := 'received bytes: '+IntToStr(res);
  Stream1.Write(buffer[0], res);

  for i := res downto 0 do begin
    Stream1.Position := Max(Stream1.GetSize-1-i, 0);
    res := Stream1.Read(search[0], sizeof(search));

    if (search[0] = EOI[0]) and (search[1] = EOI[1]) then begin
      EOIPos := Stream1.Position-sizeof(search);

      for j := 0 to EOIPos do begin
        Stream1.Position := j;
        Stream1.Read(search[0], sizeof(search));
        if (search[0] = SOI[0]) and (search[1] = SOI[1]) then begin
          SOIPos := Stream1.Position-sizeof(search);

          Stream1.Position := SOIPos;

          JPEGStream := TMemoryStream.Create;
          JPEGStream.CopyFrom(Stream1, EOIPos+sizeof(EOI)-SOIPos);
          DisplayFrame(JPEGStream);
          JPEGStream.Destroy;

          leftover := Stream1.GetSize - (EOIPos + sizeof(EOI));
          leftoverStream := TMemoryStream.Create;
          if leftover > 0 then begin
            Stream1.Position := EOIPos + sizeof(EOI);
            leftoverStream.CopyFrom(Stream1, leftover);
          end else begin
          end;
          Stream1.Destroy;
          Stream1 := leftoverStream;

          break;
        end;
      end;

      break;
    end;
  end;
end;

initialization
  {$I main.lrs}
end.

