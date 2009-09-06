#!/usr/bin/perl

=head1 NAME

server.pl - Simple webserver

=head1 SYNOPSIS

server.pl FILE [PORT]

Where I<FILE> is a JSON buddies file to serve and I<PORT> is the TCP/IP port on
which the server will listen.

=head1 DESCRIPTION

This scripts provides a custom webserver that can serve JSON files. This is a
minimalist HTTP server that's good enough for testing purposes.

=head1 AUTHOR

Emmanuel Rodriguez <emmauel.rodriguez@gmail.com>.

=cut

use warnings;
use strict;

use base 'HTTP::Server::Simple::CGI';

use File::Slurp;

exit main();


sub main {
	die "Usage: json [port]\n" unless @ARGV;
	my ($json, $port) = @ARGV;
	$port ||= 8080;
	
	my $server = __PACKAGE__->new($port);
	$server->{json} = $json;
	$server->run();

	return 0;
}


sub handle_request {
	my ($self, $cgi) = @_;

	warn "Serving page ", $ENV{REQUEST_URI}, "\n";

	print "HTTP/1.0 200 OK\r\n";

	my $content = read_file($self->{json});
	printf "Content-Length: %d\r\n", length($content);
	print "Content-Type: application/json; charset=UTF-8\r\n";
	print "\r\n";

	print $content;
}
