
$include ../shaderConventions.h

$include vertex.glsl

void main()
{
	updateVertex();
}

$define shader fragment

layout(early_fragment_tests) in;

void main()
{}
