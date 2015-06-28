#ifndef HASH
#define HASH

uint hash( uint x )
{
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}

uint hash( uvec2 v )
{
    return hash( v.x ^ hash(v.y) );
}

uint hash( uvec3 v )
{
    return hash( v.x ^ hash(v.y) ^ hash(v.z) );
}

uint hash( uvec4 v )
{
    return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w) );
}

#endif
