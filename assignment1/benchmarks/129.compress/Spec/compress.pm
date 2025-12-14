package Spec::compress;

$benchnum  = "129";
$benchname = "compress";
$exename   = "compress95";
$benchmark = "$benchnum.$benchname";
@base_exe  = qw(compress95);
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
    local (@rc);
    for ($me->inputs($size)) {
	if (($name) = m/(.*).in$/) {
	    push (@rc, "./$exename.$ext < $_ >$name.out 2>$name.err");
	}
    }
    @rc;
}

1;
