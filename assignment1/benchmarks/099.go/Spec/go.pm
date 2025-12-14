package Spec::go;

$benchnum  = "099";
$benchname = "go";
$exename   = "go";
$benchmark = "$benchnum.$benchname";
@base_exe  = qw(go);
@ISA       = qw(Spec::benchmark);

sub init {
    my ($topdir, $type, $arch) = @_;

    local($me) = bless {};
    $me->{'name'}      = $benchname;
    $me->{'num'}       = $benchnum;
    $me->{'benchmark'} = $benchmark;
    $me->{'exename'}   = $benchnum;
    $me->{'path'} =      $topdir;
    $me->{'type'} =      $type;
    $me->{'base_exe'}  = [@base_exe];

    $me->scan_data($arch);
    $me->scan_exe;
    $me->scan_setup;
    $me;
}

sub invoke {
    local ($me, $size, $ext) = @_;
    local (@rc, @temp);
    for ($me->inputs($size)) {
	if (($name) = m/(.*).in$/) {
            open (FILE, "<$name.siz") || next;
	    chomp(@temp = <FILE>);
	    close(FILE);

	    ($game_lev, $game_size) = @temp;
	    push (@rc, "./$exename.$ext $game_lev $game_size $_ > $name.out 2> $name.err");
	}
    }
    @rc;
}

1;
