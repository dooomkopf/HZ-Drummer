// Fassung von tools/gpdrums/GpDrums156.java für TuxGuitar 2.x (Pakete app.tuxguitar statt org.herac.tuxguitar),
// sonst gleich: Stream über TGSongReaderHandle, Schlagzeugspur über TGChannel.isPercussionChannel(), gleiche Ausgabe
// für gp2json.py. import_band.py nimmt diesen Leser, wenn das installierte TuxGuitar die 2.x-Pakete hat.
//
// Aufruf: java -cp /opt/tuxguitar/lib/tuxguitar-lib.jar:/opt/tuxguitar/lib/tuxguitar.jar:/opt/tuxguitar/share/plugins/tuxguitar-gtp.jar GpDrums2.java <datei> [spurnummer]
//
// Ausgabe:
//   TRACK <nr> <percussion 0/1> <name>
//   BAR <nr> <start> <länge> <zähler> <nenner> <tempo> <wiederholung-auf 0/1> <wiederholung-zu anzahl> <alternative enden, bitmaske>
//   MARKER <takt> <titel>
//   HIT <takt> <offset-ticks> <midi-note> <velocity> <ghost 0/1> <akzent 0/1>
// Ticks: 960 pro Viertel.

import app.tuxguitar.io.base.TGSongReaderHandle;
import app.tuxguitar.io.gtp.*;
import app.tuxguitar.song.factory.TGFactory;
import app.tuxguitar.song.models.*;

import java.io.*;
import java.util.Iterator;

public class GpDrums2
{
    static boolean isPercussion (TGSong song, TGTrack tr)
    {
        for (Iterator<TGChannel> it = song.getChannels(); it.hasNext();)
        {
            TGChannel ch = it.next();
            if (ch.getChannelId() == tr.getChannelId()) return ch.isPercussionChannel();
        }
        return false;
    }

    public static void main (String[] args) throws Exception
    {
        File f = new File (args[0]);
        // the version in the head decides, not the ending: many ".gp3" are Guitar Pro 1, 2 or 4 inside
        String head = "";
        try (DataInputStream d = new DataInputStream (new FileInputStream (f)))
        {
            byte[] b = new byte[31];
            d.readFully (b);
            head = new String (b, 1, Math.min (30, b[0] & 0xff), "ISO-8859-1");
        }
        catch (EOFException e) {}
        GTPSettings settings = new GTPSettings();
        GTPInputStream in = head.contains ("v5.") ? new GP5InputStream (settings)
                          : head.contains ("v4.") ? new GP4InputStream (settings)
                          : head.contains ("v2.") ? new GP2InputStream (settings)
                          : head.contains ("v1.") ? new GP1InputStream (settings)
                          : new GP3InputStream (settings);
        TGSongReaderHandle handle = new TGSongReaderHandle();
        handle.setFactory (new TGFactory());
        handle.setInputStream (new BufferedInputStream (new FileInputStream (f)));
        in.read (handle);
        TGSong song = handle.getSong();

        int wanted = args.length > 1 ? Integer.parseInt (args[1]) : -1;
        for (int t = 0; t < song.countTracks(); t++)
        {
            TGTrack tr = song.getTrack (t);
            boolean perc = isPercussion (song, tr);
            System.out.println ("TRACK " + t + " " + (perc ? 1 : 0) + " " + tr.getName().trim());
            if (wanted < 0 && perc) wanted = t;
        }
        if (wanted < 0) { System.out.println ("NODRUMS"); return; }

        TGTrack tr = song.getTrack (wanted);
        for (int m = 0; m < tr.countMeasures(); m++)
        {
            TGMeasure meas = tr.getMeasure (m);
            TGMeasureHeader h = meas.getHeader();
            TGTimeSignature ts = h.getTimeSignature();
            System.out.println ("BAR " + h.getNumber() + " " + h.getStart() + " " + h.getLength() + " "
                                + ts.getNumerator() + " " + ts.getDenominator().getValue() + " " + h.getTempo().getQuarterValue()
                                + " " + (h.isRepeatOpen() ? 1 : 0) + " " + h.getRepeatClose() + " " + h.getRepeatAlternative());
            if (h.hasMarker())
                System.out.println ("MARKER " + h.getNumber() + " " + h.getMarker().getTitle().trim());

            for (int b = 0; b < meas.countBeats(); b++)
            {
                TGBeat beat = meas.getBeat (b);
                for (int v = 0; v < beat.countVoices(); v++)
                {
                    TGVoice voice = beat.getVoice (v);
                    if (voice.isEmpty()) continue;
                    for (int k = 0; k < voice.countNotes(); k++)
                    {
                        TGNote note = voice.getNote (k);
                        if (note.isTiedNote()) continue;
                        System.out.println ("HIT " + h.getNumber() + " " + (beat.getStart() - h.getStart()) + " " + note.getValue() + " "
                                            + note.getVelocity() + " " + (note.getEffect().isGhostNote() ? 1 : 0) + " "
                                            + (note.getEffect().isAccentuatedNote() ? 1 : 0));
                    }
                }
            }
        }
    }
}
