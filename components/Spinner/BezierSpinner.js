export function BezierSpinner({ text }) {
  return (
    <>
      <div className="border-aqua rounded-l-full border-l-4 border-dotted h-11 w-11 animate-spin-bezier transform-gpu mx-auto" />
      {text && <p className="text-aqua text-xs font-bold mt-2">{text}</p>}
    </>
  )
}
