#include <cstdlib>
#include <cstring>
#include <iostream>

#include "helpers.hh"
#include "fft.hh"
#include "audio.hh"
#include "wav.hh"

using namespace std;

template <typename Signal>
tuple<float, float, size_t> amplitude( const Signal & samples )
{
    if ( samples.empty() ) {
	throw runtime_error( "samples cannot be empty" );
    }

    float sum = 0;
    float peak = -1;
    size_t peak_location = -1;

    for ( size_t i = 0; i < samples.size(); i++ ) {
	const float val = norm( samples[ i ] );
	sum += val;
	if ( val > peak ) {
	    peak = val;
	    peak_location = i;
	}
    }
    return { 20 * log10( sqrt( sum / samples.size() ) ), 20 * log10( sqrt( peak ) ), peak_location };
}

void program_body( const string & filename )
{
    SoundCard sound_card { "default", "default" };

    WavWrapper wav { filename };

    if ( wav.samples().size() % sound_card.period_size() ) {
	const size_t new_size = sound_card.period_size() * (1 + (wav.samples().size() / sound_card.period_size()));
	cerr << "Note: WAV length of " << wav.samples().size() << " is not multiple of "
	     << sound_card.period_size() << "; resizing to " << new_size << " samples.\n";
	wav.samples().resize( new_size );
    }

    ComplexSignal frequency( wav.samples().size() / 2 + 1 );
    FFTPair fft { wav.samples(), frequency };

    RealSignal input( wav.samples().size() );

    sound_card.start();

    while ( true ) {
	/* eliminate frequencies below 20 Hz */
	fft.time2frequency( wav.samples(), frequency );
	for ( unsigned int i = 0; i < frequency.size(); i++ ) {
	    if ( i * 24000.0 / frequency.size() < 20.0 ) {
		frequency[ i ] = 0;
	    }
	}
	fft.frequency2time( frequency, wav.samples() );

	/* find RMS amplitude and peak amplitudes */
	const auto [ rms, peak, peak_location ] = amplitude( wav.samples() );
	cerr << "Playing " << wav.samples().size() / 48000.0
	     << " seconds with RMS amplitude = " << rms << " dB"
	     << " and peak amplitude = " << peak << " dB"
	     << " @ " << peak_location / 48000.0 << " s.\n";

	fft.time2frequency( wav.samples(), frequency );

	const auto [ rms_freq, peak_freq, peak_location_freq ] = amplitude( frequency );
	cerr << "In frequency domain, RMS amplitude = " << rms_freq << " dB"
	     << " and peak amplitude = " << peak_freq << " dB"
	     << " @ " << peak_location_freq * 24000.0 / frequency.size() << " Hz.\n";

	/* play and record */
	sound_card.play_and_record( wav.samples(), input );
	wav.samples() = input;
    }
}

int main( const int argc, const char * argv[] )
{
    if ( argc < 0 ) { abort(); }

    if ( argc != 2 ) {
        cerr << "Usage: " << argv[ 0 ] << " filename\n";
        return EXIT_FAILURE;
    }

    try {
        program_body( argv[ 1 ] );
    } catch ( const exception & e ) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
