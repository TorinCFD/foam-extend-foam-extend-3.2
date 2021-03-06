/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | foam-extend: Open Source CFD
   \\    /   O peration     | Version:     3.2
    \\  /    A nd           | Web:         http://www.foam-extend.org
     \\/     M anipulation  | For copyright notice see file Copyright
-------------------------------------------------------------------------------
License
    This file is part of foam-extend.

    foam-extend is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License, or (at your
    option) any later version.

    foam-extend is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with foam-extend.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "timeSelector.H"
#include "ListOps.H"
#include "argList.H"
#include "objectRegistry.H"
#include "IStringStream.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::timeSelector::timeSelector()
:
    scalarRanges()
{}


Foam::timeSelector::timeSelector(Istream& is)
:
    scalarRanges(is)
{}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::timeSelector::selected(const instant& value) const
{
    return scalarRanges::selected(value.value());
}


Foam::List<bool> Foam::timeSelector::selected(const List<instant>& Times) const
{
    List<bool> lst(Times.size(), false);

    // check ranges, avoid false positive on constant/
    forAll(Times, timeI)
    {
        if (Times[timeI].name() != "constant" && selected(Times[timeI]))
        {
            lst[timeI] = true;
        }
    }

    // check specific values
    forAll(*this, rangeI)
    {
        if (operator[](rangeI).isExact())
        {
            scalar target = operator[](rangeI).value();

            int nearestIndex = -1;
            scalar nearestDiff = Foam::GREAT;

            forAll(Times, timeI)
            {
                if (Times[timeI].name() == "constant") continue;

                scalar diff = fabs(Times[timeI].value() - target);
                if (diff < nearestDiff)
                {
                    nearestDiff = diff;
                    nearestIndex = timeI;
                }
            }

            if (nearestIndex >= 0)
            {
                lst[nearestIndex] = true;
            }
        }
    }

    return lst;
}


Foam::List<Foam::instant> Foam::timeSelector::select
(
    const List<instant>& Times
) const
{
    return subset(selected(Times), Times);
}


void Foam::timeSelector::inplaceSelect
(
    List<instant>& Times
) const
{
    inplaceSubset(selected(Times), Times);
}


void Foam::timeSelector::addOptions
(
    const bool constant,
    const bool zeroTime
)
{
    if (constant)
    {
        argList::validOptions.insert("constant", "");
    }
    if (zeroTime)
    {
        argList::validOptions.insert("zeroTime", "");
    }
    argList::validOptions.insert("noZero", "");
    argList::validOptions.insert("time", "ranges");
    argList::validOptions.insert("latestTime", "");
}


Foam::List<Foam::instant> Foam::timeSelector::select
(
    const List<instant>& timeDirs,
    const argList& args
)
{
    if (timeDirs.size())
    {
        List<bool> selectTimes(timeDirs.size(), true);

        // determine locations of constant/ and 0/ directories
        label constantIdx = -1;
        label zeroIdx = -1;

        forAll(timeDirs, timeI)
        {
            if (timeDirs[timeI].name() == "constant")
            {
                constantIdx = timeI;
            }
            else if (timeDirs[timeI].value() == 0)
            {
                zeroIdx = timeI;
            }

            if (constantIdx >= 0 && zeroIdx >= 0)
            {
                break;
            }
        }

        // determine latestTime selection (if any)
        // this must appear before the -time option processing
        label latestIdx = -1;
        if (args.optionFound("latestTime"))
        {
            selectTimes = false;
            latestIdx = timeDirs.size() - 1;

            // avoid false match on constant/
            if (latestIdx == constantIdx)
            {
                latestIdx = -1;
            }
        }

        if (args.optionFound("time"))
        {
            // can match 0/, but can never match constant/
            selectTimes = timeSelector
            (
                args.optionLookup("time")()
            ).selected(timeDirs);
        }


        // add in latestTime (if selected)
        if (latestIdx >= 0)
        {
            selectTimes[latestIdx] = true;
        }

        if (constantIdx >= 0)
        {
            // only add constant/ if specifically requested
            selectTimes[constantIdx] = args.optionFound("constant");
        }

        // special treatment for 0/
        if (zeroIdx >= 0)
        {
            if (args.optionFound("noZero"))
            {
                // exclude 0/ if specifically requested
                selectTimes[zeroIdx] = false;
            }
            else if (argList::validOptions.found("zeroTime"))
            {
                // with -zeroTime enabled, drop 0/ unless specifically requested
                selectTimes[zeroIdx] = args.optionFound("zeroTime");
            }
        }

        return subset(selectTimes, timeDirs);
    }
    else
    {
        return timeDirs;
    }
}


Foam::List<Foam::instant> Foam::timeSelector::select0
(
    Time& runTime,
    const argList& args
)
{
    instantList timeDirs = timeSelector::select(runTime.times(), args);

    if (timeDirs.empty())
    {
        FatalErrorIn(args.executable())
            << "No times selected"
            << exit(FatalError);
    }

    runTime.setTime(timeDirs[0], 0);

    return timeDirs;
}


// ************************************************************************* //
