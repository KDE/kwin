#!/usr/bin/perl
{

    # keep track of current section
    my $section = "";

    # separator mode
    my $drawSeparator;
    my $separatorActiveOnly;

    while( <> )
    {

        # store current line
        my $line = $_;

        # parse section
        if( $line =~ /^\[\s*(.*)\s*\]/ )
        {
            $section = $1;
            print( $line );
            next;
        }

        # parse key and value
        if( !( $line=~ /^(.+)\s*=\s*(.+)$/ ) )
        {
            print( $line );
            next;
        }

        my $key = $1;
        my $value = $2;

        if( $key eq "VerticalOffset" )
        {
            # shadow vertical offset
            $value = int(10*$value);
            print( "$key=$value\n" );
            next;

        } elsif( $key eq "BlendColor" ) {

            # Blend style
            # delete key (renamed to BlendStyle )
            print( "# DELETE [$section]$key\n" );
            $key="BlendStyle";

            my %hash = (
                "Solid Color"=>"BlendNone",
                "Radial Gradient"=>"BlendRadial",
                "Follow Style Hint"=>"BlendFromStyle" );

            if( $hash{$value} )
            { print( "$key=$hash{$value}\n" ); }

        } elsif( $key eq "ButtonSize" ) {

            # Button size
            my %hash = (
                "Small"=>"ButtonSmall",
                "Normal"=>"ButtonDefault",
                "Large"=>"ButtonLarge",
                "Very Large"=>"ButtonVeryLarge",
                "Huge"=>"ButtonHuge" );

            if( $hash{$value} ) { print( "$key=$hash{$value}\n" ); }
            else { print( "# DELETE [$section]$key\n" ); }

        } elsif( $key eq "FrameBorder" ) {

            # frame border size
            my %hash = (
                "No Border"=>"BorderNone",
                "No Side Border"=>"BorderNoSide",
                "Tiny"=>"BorderTiny",
                "Normal"=>"BorderDefault",
                "Large"=>"BorderLarge",
                "Very Large"=>"BorderVeryLarge",
                "Huge"=>"BorderHuge",
                "Very Huge"=>"BorderVeryHuge",
                "Oversized"=>"BorderOverSized" );

            if( $hash{$value} ) { print( "$key=$hash{$value}\n" ); }
            else { print( "# DELETE [$section]$key\n" ); }

        } elsif( $key eq "SizeGripMode" ) {

            # size grip
            # delete key (renamed to DrawSizeGrip )
            print( "# DELETE [$section]$key\n" );
            $key="DrawSizeGrip";

            my %hash = (
                "Always Hide Extra Size Grip"=>"false",
                "Show Extra Size Grip When Needed"=>"true" );

            if( $hash{$value} ) { print( "$key=$hash{$value}\n" ); }

        } elsif( $key eq "TitleAlignment" ) {

            # title alignment
            my %hash = (
                "Left"=>"AlignLeft",
                "Center"=>"AlignCenter",
                "Center (Full Width)"=>"AlignCenterFullWidth",
                "Right"=>"AlignRight" );

            if( $hash{$value} ) { print( "$key=$hash{$value}\n" ); }
            else { print( "# DELETE [$section]$key\n" ); }

        } elsif( $key eq "DrawSeparator" ) {

            # separator
            print( "# DELETE [$section]$key\n" );

            $drawSeparator = $value;
            if( !( $separatorActiveOnly eq "" ) )
            {

                $key = "SeparatorMode";
                $value = "";

                if( $drawSeparator eq "false" ) { $value = "SeparatorNever"; }
                elsif( $drawSeparator eq "true" ) {

                    if( $separatorActiveOnly eq "true" ) { $value = "SeparatorActive"; }
                    elsif( $separatorActiveOnly eq "false" ) { $value = "SeparatorAlways"; }

                }

                if( !( $value eq "" ) ) { print( "$key=$value\n" ); }
                $separatorActiveOnly="";
                $drawSeparator="";
            }

        } elsif( $key eq "SeparatorActiveOnly" ) {

            # separator
            print( "# DELETE [$section]$key\n" );

            $separatorActiveOnly = $value;
            if( !( $drawSeparator eq "" ) )
            {

                $key = "SeparatorMode";
                $value = "";

                if( $drawSeparator eq "false" ) { $value = "SeparatorNever"; }
                elsif( $drawSeparator eq "true" ) {

                    if( $separatorActiveOnly eq "true" ) { $value = "SeparatorActive"; }
                    elsif( $separatorActiveOnly eq "false" ) { $value = "SeparatorAlways"; }

                }

                if( !( $value eq "" ) ) { print( "$key=$value\n" ); }
                $separatorActiveOnly="";
                $drawSeparator="";

            }

        } elsif( $key eq "Pattern" ) {

            # exception pattern
            print( "# DELETE [$section]$key\n" );

            $key="ExceptionPattern";
            print( "$key=$value\n" );

        } elsif( $key eq "Type" ) {

            # exception type
            print( "# DELETE [$section]$key\n" );
            $key = "ExceptionType";
            my %hash = (
                "Window Class Name"=>"ExceptionWindowClassName",
                "Window Title"=>"ExceptionWindowTitle" );

            if( $hash{$value} ) { print( "$key=$hash{$value}\n" ); }

        } elsif(
            ( $key eq "CenterTitleOnFullWidth" ) ||
            ( $key eq "UseDropShadows" ) ||
            ( $key eq "UseOxygenShadows" ) ) {

            # obsolete flags
            print( "# DELETE [$section]$key\n" );


        } else {

            print( $line );

        }

    }

}
