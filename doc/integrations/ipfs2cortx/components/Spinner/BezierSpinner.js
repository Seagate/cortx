export function BezierSpinner({ text, radius }) {
  const spinnerWidth = radius ? 2 * radius : 11
  return (
    <>
      <div
        className={`border-aqua rounded-l-full border-l-4 border-dotted h-${spinnerWidth} w-${spinnerWidth} animate-spin-bezier transform-gpu mx-auto`}
      />
      {text && <p className="text-aqua text-xs font-bold mt-2">{text}</p>}
    </>
  )
}
